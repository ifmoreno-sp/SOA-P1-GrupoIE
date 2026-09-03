#include "cli.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Toma el valor que sigue a una bandera. Un valor que empieza con '-' se trata
 * como bandera faltante: ningun parametro de la interfaz acepta negativos. */
static const char *next_value(int argc, char *argv[], int *i, const char *flag,
                              char errbuf[CLI_ERRBUF_SIZE])
{
    if (*i + 1 >= argc || argv[*i + 1][0] == '-') {
        snprintf(errbuf, CLI_ERRBUF_SIZE, "%s requiere un valor", flag);
        return NULL;
    }
    (*i)++;
    return argv[*i];
}

/* Convierte un string a un entero sin signo de 64 bits. */
static int parse_uint64(const char *text, uint64_t *out)
{
    if (text == NULL || *text == '\0') {
        return -1;
    }

    char *endptr = NULL;
    errno = 0;
    unsigned long long value = strtoull(text, &endptr, 10);

    if (errno == ERANGE || endptr == text || *endptr != '\0') {
        return -1;
    }

    *out = (uint64_t)value;
    return 0;
}

/* Convierte un string a un entero sin signo de 32 bits. */
static int parse_uint32(const char *text, uint32_t *out)
{
    uint64_t value = 0;
    if (parse_uint64(text, &value) != 0 || value > UINT32_MAX) {
        return -1;
    }
    *out = (uint32_t)value;
    return 0;
}

const char *cli_usage(void)
{
    return "uso: lottery_scheduler --input <csv> --mode <cooperative|quantum>\n"
           "                       (--quantum <Q> | --slice-percent <P>)\n"
           "                       --seed <n != 0> --log <csv>\n"
           "                       [--summary <csv>] [--max-dispatches <N>]";
}

/* Parsea los argumentos de la línea de comandos. */
int cli_parse(int argc, char *argv[], CliOptions *opts,
              char errbuf[CLI_ERRBUF_SIZE])
{
    memset(opts, 0, sizeof(*opts));

    const char *mode_str = NULL;
    const char *quantum_str = NULL;
    const char *slice_str = NULL;
    const char *seed_str = NULL;
    const char *max_dispatches_str = NULL;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char **target = NULL;

        if (strcmp(arg, "--input") == 0) {
            target = &opts->input_path;
        } else if (strcmp(arg, "--log") == 0) {
            target = &opts->log_path;
        } else if (strcmp(arg, "--summary") == 0) {
            target = &opts->summary_path;
        } else if (strcmp(arg, "--mode") == 0) {
            target = &mode_str;
        } else if (strcmp(arg, "--quantum") == 0) {
            target = &quantum_str;
        } else if (strcmp(arg, "--slice-percent") == 0) {
            target = &slice_str;
        } else if (strcmp(arg, "--seed") == 0) {
            target = &seed_str;
        } else if (strcmp(arg, "--max-dispatches") == 0) {
            target = &max_dispatches_str;
        } else {
            snprintf(errbuf, CLI_ERRBUF_SIZE, "argumento desconocido: %s", arg);
            return -1;
        }

        if (*target != NULL) {
            snprintf(errbuf, CLI_ERRBUF_SIZE, "%s aparece mas de una vez", arg);
            return -1;
        }

        *target = next_value(argc, argv, &i, arg, errbuf);
        if (*target == NULL) {
            return -1;
        }
    }

    if (opts->input_path == NULL) {
        snprintf(errbuf, CLI_ERRBUF_SIZE, "falta --input <csv>");
        return -1;
    }
    if (opts->log_path == NULL) {
        snprintf(errbuf, CLI_ERRBUF_SIZE, "falta --log <csv>");
        return -1;
    }
    if (mode_str == NULL) {
        snprintf(errbuf, CLI_ERRBUF_SIZE, "falta --mode <cooperative|quantum>");
        return -1;
    }

    if (strcmp(mode_str, "cooperative") == 0) {
        opts->mode = MODE_COOPERATIVE;
        if (quantum_str != NULL) {
            snprintf(errbuf, CLI_ERRBUF_SIZE,
                     "--quantum no aplica en modo cooperative");
            return -1;
        }
        if (slice_str == NULL) {
            snprintf(errbuf, CLI_ERRBUF_SIZE,
                     "modo cooperative requiere --slice-percent <1-100>");
            return -1;
        }
        if (parse_uint32(slice_str, &opts->slice_percent) != 0 ||
            opts->slice_percent < 1 || opts->slice_percent > 100) {
            snprintf(errbuf, CLI_ERRBUF_SIZE,
                     "--slice-percent debe ser un entero entre 1 y 100, recibido '%s'",
                     slice_str);
            return -1;
        }
    } else if (strcmp(mode_str, "quantum") == 0) {
        opts->mode = MODE_QUANTUM;
        if (slice_str != NULL) {
            snprintf(errbuf, CLI_ERRBUF_SIZE,
                     "--slice-percent no aplica en modo quantum");
            return -1;
        }
        if (quantum_str == NULL) {
            snprintf(errbuf, CLI_ERRBUF_SIZE, "modo quantum requiere --quantum <Q>");
            return -1;
        }
        if (parse_uint32(quantum_str, &opts->quantum) != 0 ||
            opts->quantum == 0) {
            snprintf(errbuf, CLI_ERRBUF_SIZE,
                     "--quantum debe ser un entero positivo, recibido '%s'",
                     quantum_str);
            return -1;
        }
    } else {
        snprintf(errbuf, CLI_ERRBUF_SIZE,
                 "--mode invalido: '%s' (use cooperative o quantum)", mode_str);
        return -1;
    }

    if (seed_str == NULL) {
        snprintf(errbuf, CLI_ERRBUF_SIZE, "falta --seed <n != 0>");
        return -1;
    }
    if (parse_uint32(seed_str, &opts->seed) != 0 || opts->seed == 0) {
        snprintf(errbuf, CLI_ERRBUF_SIZE,
                 "--seed debe ser un entero positivo distinto de cero, recibido '%s'",
                 seed_str);
        return -1;
    }

    if (max_dispatches_str != NULL) {
        if (parse_uint64(max_dispatches_str, &opts->max_dispatches) != 0 ||
            opts->max_dispatches == 0) {
            snprintf(errbuf, CLI_ERRBUF_SIZE,
                     "--max-dispatches debe ser un entero positivo, recibido '%s'",
                     max_dispatches_str);
            return -1;
        }
        opts->has_max_dispatches = 1;
    }

    return 0;
}
