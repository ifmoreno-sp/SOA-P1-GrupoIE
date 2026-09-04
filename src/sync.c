#include "sync.h"

#include <assert.h>

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

void sync_wait_for_dispatch(Sync *sync, Task *task)
{
    pthread_mutex_lock(&sync->mutex);
    while (task->state != TASK_RUNNING) {
        pthread_cond_wait(&task->cond_worker, &sync->mutex);
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
