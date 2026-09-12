#ifndef YIELD_CONFIG_H
#define YIELD_CONFIG_H

#include <stddef.h>

#include "task.h"

#define YIELD_CONFIG_ERRBUF_SIZE 256

/* Carga la configuracion opcional de cesion temprana forzada (extension M9,
 * bandera --yield-config): un CSV con encabezado exacto
 * "task_id,yield_percent" y una fila por cada tarea que debe ceder
 * temprano, con su fraccion (entero 1..99, ver task_set_yield_config).
 *
 * Valida el archivo COMPLETO antes de tocar tasks[] (sin id duplicado
 * dentro del archivo, yield_percent en rango, cada task_id existe en
 * tasks[0..task_count)): en caso de error, ninguna tarea queda modificada,
 * igual que csv_parser_load no deja memoria a medio construir ante un
 * error. Solo entonces llama task_set_yield_config para cada fila.
 *
 * Precondiciones: path, tasks, errbuf != NULL.
 * Retorna 0 en exito. En error retorna distinto de cero y escribe un
 * mensaje descriptivo en errbuf. */
int yield_config_load(const char *path, Task *tasks, size_t task_count,
                       char errbuf[YIELD_CONFIG_ERRBUF_SIZE]);

#endif /* YIELD_CONFIG_H */
