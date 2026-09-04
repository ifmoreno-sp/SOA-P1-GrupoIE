#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <stddef.h>

#include "task.h"

#define CSV_PARSER_ERRBUF_SIZE 256
#define CSV_PARSER_MIN_TASKS 5
#define CSV_PARSER_MAX_TASKS 25

/* Carga las tareas desde un CSV con encabezado exacto "id,tickets,work_units".
 *
 * Validaciones aplicadas: entre CSV_PARSER_MIN_TASKS y CSV_PARSER_MAX_TASKS
 * filas, ids unicos, tickets y work_units enteros positivos representables en
 * uint32_t, y suma de tickets acumulada en uint64_t dentro de [1, UINT32_MAX].
 *
 * Precondiciones: path, out_tasks, out_count y errbuf != NULL.
 * En exito retorna 0, *out_tasks apunta a un arreglo de *out_count tareas que
 * el llamante debe liberar con free(). En error retorna un valor distinto de
 * cero, escribe un mensaje descriptivo en errbuf, deja *out_tasks en NULL y
 * *out_count en 0, y no deja memoria reservada. */
int csv_parser_load(const char *path, Task **out_tasks, size_t *out_count,
                    char errbuf[CSV_PARSER_ERRBUF_SIZE]);

#endif /* CSV_PARSER_H */
