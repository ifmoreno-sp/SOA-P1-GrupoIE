#ifndef CLI_H
#define CLI_H

#include <stdint.h>

#define CLI_ERRBUF_SIZE 256

typedef enum {
    MODE_COOPERATIVE,
    MODE_QUANTUM
} SchedulerMode;

typedef struct {
    const char *input_path;
    const char *log_path;
    const char *summary_path;  /* NULL si no se especifico: es opcional. */
    SchedulerMode mode;
    uint32_t quantum;          /* Valido solo con MODE_QUANTUM. */
    uint32_t slice_percent;    /* Valido solo con MODE_COOPERATIVE, 1..100. */
    uint32_t seed;             /* Distinta de cero: xorshift32 lo exige. */
    uint64_t max_dispatches;   /* Valida solo si has_max_dispatches != 0. */
    int has_max_dispatches;
} CliOptions;

/* Parsea los argumentos de linea de comandos segun la interfaz del enunciado:
 *   --input <ruta> --mode <cooperative|quantum> --seed <n != 0> --log <ruta>
 *   [--quantum <Q> | --slice-percent <P>] [--summary <ruta>]
 *   [--max-dispatches <N>]
 *
 * --quantum es obligatorio en modo quantum y --slice-percent en modo
 * cooperative; pasar el que no corresponde al modo es un error.
 *
 * Precondiciones: opts y errbuf != NULL.
 * Retorna 0 y llena *opts en exito. En error retorna distinto de cero, escribe
 * un mensaje descriptivo en errbuf y *opts queda sin uso valido. */
int cli_parse(int argc, char *argv[], CliOptions *opts,
              char errbuf[CLI_ERRBUF_SIZE]);

/* Texto de uso, para acompanar los mensajes de error. */
const char *cli_usage(void);

#endif /* CLI_H */
