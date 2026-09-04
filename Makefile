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
.PHONY: all test test-rng test-workload clean

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
# pruebas unitarias de los módulos que ya las tienen (rng y workload).
test: all test-rng test-workload
	bash tests/test_input_validation.sh

test-rng: $(BUILD_DIR)/test_rng
	./$(BUILD_DIR)/test_rng

$(BUILD_DIR)/test_rng: tests/test_rng.c src/rng.c include/rng.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_rng.c src/rng.c

test-workload: $(BUILD_DIR)/test_workload
	./$(BUILD_DIR)/test_workload

$(BUILD_DIR)/test_workload: tests/test_workload.c src/task.c src/workload.c include/task.h include/workload.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_workload.c src/task.c src/workload.c

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
