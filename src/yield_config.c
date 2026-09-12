#include "yield_config.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv_parser.h"

static const char *const YIELD_CONFIG_HEADER = "task_id,yield_percent";

static void trim_eol(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

/* Separa una linea en exactamente 2 campos por coma, preservando campos
 * vacios (mismo criterio que split_csv_row en csv_parser.c, y por la misma
 * razon: strtok colapsaria comas consecutivas). */
static int split_row(char *line, char *fields[2])
{
    char *comma = strchr(line, ',');
    if (comma == NULL) {
        return -1;
    }
    *comma = '\0';
    fields[0] = line;
    fields[1] = comma + 1;
    if (strchr(fields[1], ',') != NULL) {
        return -1;
    }
    return 0;
}

static int parse_uint32_field(const char *field, uint32_t *out)
{
    if (field == NULL || *field == '\0') {
        return -1;
    }

    char *endptr = NULL;
    errno = 0;
    unsigned long value = strtoul(field, &endptr, 10);

    if (errno == ERANGE || endptr == field || *endptr != '\0' || value > UINT32_MAX) {
        return -1;
    }

    *out = (uint32_t)value;
    return 0;
}

typedef struct {
    uint32_t task_id;
    uint32_t yield_percent;
} YieldEntry;

int yield_config_load(const char *path, Task *tasks, size_t task_count,
                       char errbuf[YIELD_CONFIG_ERRBUF_SIZE])
{
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE, "no se pudo abrir '%s': %s",
                 path, strerror(errno));
        return -1;
    }

    char *line = NULL;
    size_t line_cap = 0;

    if (getline(&line, &line_cap, fp) < 0) {
        snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE,
                 "archivo vacio o encabezado ilegible: '%s'", path);
        free(line);
        fclose(fp);
        return -1;
    }

    trim_eol(line);
    if (strcmp(line, YIELD_CONFIG_HEADER) != 0) {
        snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE,
                 "encabezado invalido, se esperaba '%s'", YIELD_CONFIG_HEADER);
        free(line);
        fclose(fp);
        return -1;
    }

    /* A lo sumo una fila por tarea real tiene sentido; mas que eso implica
     * un id repetido, que se rechaza explicitamente mas abajo. */
    YieldEntry entries[CSV_PARSER_MAX_TASKS];
    size_t entry_count = 0;
    int failed = 0;

    while (!failed && getline(&line, &line_cap, fp) >= 0) {
        trim_eol(line);
        if (line[0] == '\0') {
            continue;
        }

        size_t row = entry_count + 2; /* la fila 1 es el encabezado */

        if (entry_count >= CSV_PARSER_MAX_TASKS) {
            snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE,
                     "demasiadas filas: el maximo permitido es %d", CSV_PARSER_MAX_TASKS);
            failed = 1;
            break;
        }

        char *fields[2];
        if (split_row(line, fields) != 0) {
            snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE,
                     "fila %zu: se esperaban exactamente 2 columnas", row);
            failed = 1;
            break;
        }

        uint32_t task_id = 0;
        uint32_t yield_percent = 0;
        if (parse_uint32_field(fields[0], &task_id) != 0) {
            snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE,
                     "fila %zu: task_id invalido '%s'", row, fields[0]);
            failed = 1;
            break;
        }
        if (parse_uint32_field(fields[1], &yield_percent) != 0 ||
            yield_percent < 1 || yield_percent > 99) {
            snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE,
                     "fila %zu: yield_percent debe ser un entero entre 1 y 99, recibido '%s'",
                     row, fields[1]);
            failed = 1;
            break;
        }

        for (size_t i = 0; i < entry_count; i++) {
            if (entries[i].task_id == task_id) {
                snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE,
                         "fila %zu: task_id %u repetido dentro de '%s'", row, task_id, path);
                failed = 1;
                break;
            }
        }
        if (failed) {
            break;
        }

        entries[entry_count].task_id = task_id;
        entries[entry_count].yield_percent = yield_percent;
        entry_count++;
    }

    free(line);
    fclose(fp);
    if (failed) {
        return -1;
    }

    /* Segunda pasada: localizar cada task_id en tasks[] ANTES de activar
     * ninguna cesion, para no dejar algunas tareas modificadas si una
     * entrada posterior resulta invalida. */
    for (size_t i = 0; i < entry_count; i++) {
        int found = 0;
        for (size_t t = 0; t < task_count; t++) {
            if (tasks[t].id == entries[i].task_id) {
                found = 1;
                break;
            }
        }
        if (!found) {
            snprintf(errbuf, YIELD_CONFIG_ERRBUF_SIZE,
                     "task_id %u en '%s' no existe en el CSV de entrada",
                     entries[i].task_id, path);
            return -1;
        }
    }

    for (size_t i = 0; i < entry_count; i++) {
        for (size_t t = 0; t < task_count; t++) {
            if (tasks[t].id == entries[i].task_id) {
                task_set_yield_config(&tasks[t], entries[i].yield_percent);
                break;
            }
        }
    }

    return 0;
}
