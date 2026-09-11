# Flags exigidos por el enunciado: C17 estricto + POSIX threads.
# _POSIX_C_SOURCE expone getline() y strtok_r(): con -std=c17 estricto glibc
# las oculta, porque son POSIX y no parte de C17.
CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -Werror -pthread -D_POSIX_C_SOURCE=200809L

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
# El enunciado pide un ejecutable llamado lottery_scheduler y lo invoca como
# ./lottery_scheduler desde la raiz del repo.
TARGET = lottery_scheduler

# Descubre todos los .c de src/ y deriva su .o correspondiente en build/,
# para no tener que listar cada archivo fuente a mano.
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Ninguno de estos targets produce un archivo con su propio nombre.
.PHONY: all test test-rng test-workload test-concurrency test-scheduler test-modes test-results asan tsan clean

all: $(TARGET)

# Enlaza el binario final a partir de los objetos ya compilados.
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(OBJS)

# Compila cada .c a su .o; el order-only prerequisite (| $(BUILD_DIR))
# asegura que build/ exista sin invalidar el .o si solo cambia el timestamp del directorio.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Corre las pruebas de validación de entrada (CSV y argumentos) y las
# pruebas unitarias de los módulos que ya las tienen (rng, workload, el
# núcleo de concurrencia, el scheduler y los modos de ejecución).
test: all test-rng test-workload test-concurrency test-scheduler test-modes test-results
	bash tests/test_input_validation.sh

test-rng: $(BUILD_DIR)/test_rng
	./$(BUILD_DIR)/test_rng

$(BUILD_DIR)/test_rng: tests/test_rng.c src/rng.c include/rng.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_rng.c src/rng.c

test-workload: $(BUILD_DIR)/test_workload
	./$(BUILD_DIR)/test_workload

$(BUILD_DIR)/test_workload: tests/test_workload.c src/task.c src/workload.c include/task.h include/workload.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_workload.c src/task.c src/workload.c

test-concurrency: $(BUILD_DIR)/test_concurrency
	./$(BUILD_DIR)/test_concurrency

# src/rng.c es dependencia de sync.c: sync_select_winner sortea boletos
# con el RNG del proyecto. include/cli.h es dependencia de worker.h: los
# modos de ejecucion (M6) viven en WorkerArgs como un SchedulerMode.
$(BUILD_DIR)/test_concurrency: tests/test_concurrency.c src/task.c src/sync.c src/worker.c src/workload.c src/rng.c include/task.h include/sync.h include/worker.h include/workload.h include/rng.h include/cli.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_concurrency.c src/task.c src/sync.c src/worker.c src/workload.c src/rng.c

test-scheduler: $(BUILD_DIR)/test_scheduler
	./$(BUILD_DIR)/test_scheduler

$(BUILD_DIR)/test_scheduler: tests/test_scheduler.c src/task.c src/sync.c src/worker.c src/workload.c src/rng.c src/scheduler.c include/task.h include/sync.h include/worker.h include/workload.h include/rng.h include/scheduler.h include/csv_parser.h include/cli.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_scheduler.c src/task.c src/sync.c src/worker.c src/workload.c src/rng.c src/scheduler.c

test-modes: $(BUILD_DIR)/test_modes
	./$(BUILD_DIR)/test_modes

# Mismas dependencias de src/rng.c/include/cli.h que test-concurrency, por
# la misma razon (sync.c llama al RNG; worker.h expone SchedulerMode).
$(BUILD_DIR)/test_modes: tests/test_execution_modes.c src/task.c src/sync.c src/worker.c src/workload.c src/rng.c include/task.h include/sync.h include/worker.h include/workload.h include/rng.h include/cli.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_execution_modes.c src/task.c src/sync.c src/worker.c src/workload.c src/rng.c

# Reconstruye y corre toda la suite (mismos targets que test) con
# AddressSanitizer + UndefinedBehaviorSanitizer. Usa su propio BUILD_DIR
# para no mezclar objetos con los de una compilacion normal; TARGET no
# cambia, asi que tests/test_input_validation.sh (que invoca ./lottery_scheduler
# a secas) sigue funcionando sin modificaciones.
test-results: $(BUILD_DIR)/test_results
	./$(BUILD_DIR)/test_results

$(BUILD_DIR)/test_results: tests/test_results.c src/task.c src/workload.c src/results.c include/task.h include/workload.h include/results.h include/scheduler.h include/cli.h include/sync.h include/rng.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_results.c src/task.c src/workload.c src/results.c

asan:
	$(MAKE) BUILD_DIR=build-asan CFLAGS="$(CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer" test

# Igual que asan, pero con ThreadSanitizer. No se combinan en el mismo
# binario (son instrumentaciones incompatibles entre si), por eso son dos
# targets separados con su propio BUILD_DIR cada uno.
tsan:
	$(MAKE) BUILD_DIR=build-tsan CFLAGS="$(CFLAGS) -fsanitize=thread" test

clean:
	rm -rf $(BUILD_DIR) build-asan build-tsan $(TARGET)
