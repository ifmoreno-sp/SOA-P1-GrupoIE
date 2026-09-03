#include "csv_parser.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const CSV_HEADER = "id,tickets,work_units";

/* Elimina los caracteres de fin de línea '\n' y '\r' al final de la cadena. */
static void trim_eol(char *line)
{
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

/* Convierte un string a un entero sin signo de 32 bits. */
/* Separa una linea en exactamente 3 campos por coma, preservando campos
 * vacios (a diferencia de strtok, que colapsa comas consecutivas y por lo
 * tanto no puede distinguir "1,,1000" de "1,1000"). Modifica line in-place
 * insertando '\0' en las comas de separacion.
 * Retorna 0 y llena fields[0..2] en exito. Retorna -1 si faltan columnas
 * (menos de 3) o si sobran (una coma extra despues de la tercera). */
static int split_csv_row(char *line, char *fields[3])
{
    char *start = line;
    for (int i = 0; i < 2; i++) {
        char *comma = strchr(start, ',');
        if (comma == NULL) {
            return -1;
        }
        *comma = '\0';
        fields[i] = start;
        start = comma + 1;
    }
    if (strchr(start, ',') != NULL) {
        return -1;
    }
    fields[2] = start;
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

    if (errno == ERANGE || endptr == field || *endptr != '\0' ||
        value > UINT32_MAX) {
        return -1;
    }

    *out = (uint32_t)value;
    return 0;
}

/* Carga las tareas desde un CSV de entrada. */
int csv_parser_load(const char *path, Task **out_tasks, size_t *out_count,
                    char errbuf[CSV_PARSER_ERRBUF_SIZE])
{
    *out_tasks = NULL;
    *out_count = 0;

    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE, "no se pudo abrir '%s': %s",
                 path, strerror(errno));
        return -1;
    }

    char *line = NULL;
    size_t line_cap = 0;

    if (getline(&line, &line_cap, fp) < 0) {
        snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                 "archivo vacio o encabezado ilegible: '%s'", path);
        free(line);
        fclose(fp);
        return -1;
    }

    trim_eol(line);
    if (strcmp(line, CSV_HEADER) != 0) {
        snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                 "encabezado invalido, se esperaba '%s'", CSV_HEADER);
        free(line);
        fclose(fp);
        return -1;
    }

    Task *tasks = NULL;
    size_t count = 0;
    size_t capacity = 0;
    uint64_t total_tickets = 0;
    int failed = 0;

    while (!failed && getline(&line, &line_cap, fp) >= 0) {
        trim_eol(line);
        if (line[0] == '\0') {
            continue;
        }

        /* La fila del archivo es count + 2: la 1 es el encabezado. */
        size_t row = count + 2;

        if (count >= CSV_PARSER_MAX_TASKS) {
            snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                     "demasiadas tareas: el maximo permitido es %d",
                     CSV_PARSER_MAX_TASKS);
            failed = 1;
            break;
        }

        char *fields[3];
        if (split_csv_row(line, fields) != 0) {
            snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                     "fila %zu: se esperaban exactamente 3 columnas", row);
            failed = 1;
            break;
        }
        char *id_str = fields[0];
        char *tickets_str = fields[1];
        char *work_str = fields[2];

        uint32_t id = 0;
        uint32_t tickets = 0;
        uint32_t work_units = 0;

        if (parse_uint32_field(id_str, &id) != 0) {
            snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                     "fila %zu: id invalido '%s'", row, id_str);
            failed = 1;
            break;
        }
        if (parse_uint32_field(tickets_str, &tickets) != 0 || tickets == 0) {
            snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                     "fila %zu: tickets debe ser un entero positivo, recibido '%s'",
                     row, tickets_str);
            failed = 1;
            break;
        }
        if (parse_uint32_field(work_str, &work_units) != 0 || work_units == 0) {
            snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                     "fila %zu: work_units debe ser un entero positivo, recibido '%s'",
                     row, work_str);
            failed = 1;
            break;
        }

        for (size_t j = 0; j < count; j++) {
            if (tasks[j].id == id) {
                snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                         "id duplicado: %u aparece en las filas %zu y %zu", id,
                         j + 2, row);
                failed = 1;
                break;
            }
        }
        if (failed) {
            break;
        }

        total_tickets += tickets;
        if (total_tickets > UINT32_MAX) {
            snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                     "fila %zu: la suma de tickets activos excede UINT32_MAX",
                     row);
            failed = 1;
            break;
        }

        if (count == capacity) {
            size_t new_capacity = (capacity == 0) ? 8 : capacity * 2;
            Task *resized = realloc(tasks, new_capacity * sizeof(*tasks));
            if (resized == NULL) {
                snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                         "sin memoria suficiente para cargar las tareas");
                failed = 1;
                break;
            }
            tasks = resized;
            capacity = new_capacity;
        }

        /* Inicializa la tarea con los valores del CSV. */
        task_init(&tasks[count], id, tickets, work_units);
        count++;
    }

    free(line);
    fclose(fp);

    if (!failed && count < CSV_PARSER_MIN_TASKS) {
        snprintf(errbuf, CSV_PARSER_ERRBUF_SIZE,
                 "se requieren al menos %d tareas, se encontraron %zu",
                 CSV_PARSER_MIN_TASKS, count);
        failed = 1;
    }

    if (failed) {
        free(tasks);
        return -1;
    }

    *out_tasks = tasks;
    *out_count = count;
    return 0;
}
