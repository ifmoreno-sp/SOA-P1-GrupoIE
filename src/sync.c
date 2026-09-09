#include "sync.h"

#include <assert.h>
#include <stdint.h>

int sync_init(Sync *sync)
{
    if (pthread_mutex_init(&sync->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&sync->cond_scheduler, NULL) != 0) {
        pthread_mutex_destroy(&sync->mutex);
        return -1;
    }
    sync->event_ready = 0;
    sync->stop_requested = 0;
    return 0;
}

void sync_destroy(Sync *sync)
{
    pthread_mutex_destroy(&sync->mutex);
    pthread_cond_destroy(&sync->cond_scheduler);
}

void sync_dispatch(Sync *sync, Task *tasks, size_t task_count, size_t winner_index)
{
    assert(winner_index < task_count);

    pthread_mutex_lock(&sync->mutex);

    assert(tasks[winner_index].state == TASK_READY);
    for (size_t i = 0; i < task_count; i++) {
        assert(tasks[i].state != TASK_RUNNING);
    }

    tasks[winner_index].state = TASK_RUNNING;
    tasks[winner_index].dispatch_count++;
    pthread_cond_signal(&tasks[winner_index].cond_worker);

    pthread_mutex_unlock(&sync->mutex);
}

void sync_wait_for_event(Sync *sync)
{
    pthread_mutex_lock(&sync->mutex);
    while (!sync->event_ready) {
        pthread_cond_wait(&sync->cond_scheduler, &sync->mutex);
    }
    sync->event_ready = 0;
    pthread_mutex_unlock(&sync->mutex);
}

int sync_wait_for_dispatch(Sync *sync, Task *task)
{
    pthread_mutex_lock(&sync->mutex);
    while (task->state != TASK_RUNNING && !sync->stop_requested) {
        pthread_cond_wait(&task->cond_worker, &sync->mutex);
    }
    int stopped = (task->state != TASK_RUNNING);
    pthread_mutex_unlock(&sync->mutex);
    return stopped ? -1 : 0;
}

void sync_request_stop(Sync *sync, Task *tasks, size_t task_count)
{
    pthread_mutex_lock(&sync->mutex);

    sync->stop_requested = 1;
    for (size_t i = 0; i < task_count; i++) {
        assert(tasks[i].state != TASK_RUNNING);
        if (tasks[i].state == TASK_READY) {
            pthread_cond_signal(&tasks[i].cond_worker);
        }
    }

    pthread_mutex_unlock(&sync->mutex);
}

void sync_finish_turn(Sync *sync, Task *task, TaskState next_state)
{
    assert(next_state == TASK_READY || next_state == TASK_FINISHED);

    pthread_mutex_lock(&sync->mutex);
    task->state = next_state;
    sync->event_ready = 1;
    pthread_cond_signal(&sync->cond_scheduler);
    pthread_mutex_unlock(&sync->mutex);
}

Selection sync_select_winner(Sync *sync, Task *tasks, size_t task_count, Rng *rng)
{
    pthread_mutex_lock(&sync->mutex);

    uint64_t active_tickets = 0;
    for (size_t i = 0; i < task_count; i++) {
        assert(tasks[i].state != TASK_RUNNING);
        if (tasks[i].state == TASK_READY) {
            active_tickets += tasks[i].tickets;
        }
    }

    Selection sel = {0};
    if (active_tickets == 0) {
        sel.index = task_count;
        pthread_mutex_unlock(&sync->mutex);
        return sel;
    }

    /* active_tickets es la suma de un subconjunto de los tickets validados
     * por csv_parser_load, que ya garantiza que la suma TOTAL cabe en
     * [1, UINT32_MAX]; un subconjunto no puede excederla. */
    assert(active_tickets <= UINT32_MAX);
    uint32_t ticket = rng_draw_ticket(rng, (uint32_t)active_tickets);

    uint64_t accum = 0;
    size_t winner = task_count;
    for (size_t i = 0; i < task_count; i++) {
        if (tasks[i].state != TASK_READY) {
            continue;
        }
        accum += tasks[i].tickets;
        if (ticket <= accum) {
            winner = i;
            break;
        }
    }
    /* ticket esta en [1, active_tickets] y accum recorre exactamente esa
     * suma sobre las tareas READY: siempre cae en alguna. */
    assert(winner < task_count);

    sel.index = winner;
    sel.winning_ticket = ticket;
    sel.active_tickets = active_tickets;
    sel.completed_units_before = tasks[winner].completed_units;

    pthread_mutex_unlock(&sync->mutex);
    return sel;
}
