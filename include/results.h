#ifndef RESULTS_H
#define RESULTS_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "scheduler.h"
#include "task.h"

/* Estadisticas acumuladas por tarea a lo largo de la ejecucion, que
 * complementan lo que ya vive en Task (tickets, work_units,
 * completed_units, dispatch_count, pi_approx: esos se leen directo de
 * Task al armar el resumen, no se duplican aqui). Se llenan
 * incrementalmente via results_record_event, una entrada por indice de
 * tasks[]. */
typedef struct {
    uint64_t first_dispatch; /* 0 si la tarea nunca gano un despacho */
    uint64_t last_dispatch;  /* 0 si la tarea nunca gano un despacho */
    uint64_t run_units_sum;  /* unidades ejecutadas por esta tarea en toda
                               * la ventana observada */
} TaskStats;

/* Contexto que se pasa como observer_ctx a scheduler_run. Escribe cada
 * evento en `log_file` (formato CSV) y acumula TaskStats por tarea. El
 * llamador es dueno de toda la memoria referenciada (tasks, stats,
 * log_file): ResultsContext solo la usa, no la libera ni la cierra. */
typedef struct {
    FILE *log_file;
    const Task *tasks; /* para mapear winner_id -> indice en stats[] */
    size_t task_count;
    TaskStats *stats;        /* [task_count], inicializado en cero por el llamador */
    uint64_t total_run_units; /* suma acumulada sobre todas las tareas */
} ResultsContext;

/* Inicializa ctx con los punteros dados. Precondicion: stats apunta a un
 * arreglo de task_count TaskStats ya puesto en cero (calloc o
 * equivalente); log_file ya esta abierto en modo escritura. No escribe
 * nada todavia. */
void results_context_init(ResultsContext *ctx, FILE *log_file,
                           const Task *tasks, size_t task_count, TaskStats *stats);

/* Escribe el encabezado del CSV de eventos (las 7 columnas del enunciado)
 * en `file`. Llamar una vez, antes de que scheduler_run empiece a
 * despachar. */
void results_write_log_header(FILE *file);

/* DispatchObserver compatible con scheduler_run (ver DispatchObserver en
 * scheduler.h): escribe una fila del log de eventos en ctx->log_file y
 * actualiza ctx->stats[i] para la tarea ganadora (primer/ultimo despacho,
 * suma de run_units) y ctx->total_run_units.
 *
 * `ctx` debe apuntar a un ResultsContext valido (no NULL). */
void results_record_event(const DispatchEvent *event, void *ctx);

/* Llamada una vez por cada tarea que haya quedado en TASK_READY cuando
 * scheduler_run retorna (es decir, --max-dispatches corto la observacion
 * antes de que esa tarea terminara). Escribe una fila adicional en el log
 * con state_after = STOPPED, documentando que la tarea quedo a medias por
 * el limite de observacion y no por un despacho real:
 *
 *   - winning_ticket = 0 y active_tickets = 0: no hubo sorteo para esta
 *     fila (un despacho real siempre tiene ambos >= 1), asi que estos
 *     valores marcan la fila como sintetica sin necesidad de inspeccionar
 *     state_after.
 *   - run_units = 0: no se ejecuto trabajo nuevo.
 *   - completed_units = el progreso real de la tarea al momento del corte.
 *
 * `dispatch_number` es el mismo para todas las tareas detenidas en la
 * misma corrida (se recomienda pasar el total de despachos realizados):
 * todas representan el mismo instante de corte, no despachos
 * independientes. */
void results_write_stopped_row(FILE *file, const Task *task, uint64_t dispatch_number);

/* Escribe el encabezado del CSV de resumen final en `file`. */
void results_write_summary_header(FILE *file);

/* Escribe una fila de resumen para `task` en `file`: tickets iniciales,
 * trabajo asignado/completado, despachos ganados, primer/ultimo despacho,
 * aproximacion final de pi y observed_share.
 *
 * observed_share = stats->run_units_sum / total_run_units (unidades
 * ejecutadas por esta tarea sobre el total ejecutado por todas, dentro de
 * la ventana observada). Si total_run_units es 0 (no hubo ningun
 * despacho), se reporta 0.0 en vez de dividir entre cero. */
void results_write_summary_row(FILE *file, const Task *task, const TaskStats *stats,
                                uint64_t total_run_units);

/* Imprime en stdout, en formato de tabla legible, el mismo contenido que
 * results_write_summary_row para las `task_count` tareas en `tasks`. */
void results_print_console_summary(const Task *tasks, const TaskStats *stats,
                                    size_t task_count, uint64_t total_run_units);

#endif /* RESULTS_H */
