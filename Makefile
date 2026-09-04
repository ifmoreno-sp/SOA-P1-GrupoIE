# Flags exigidos por el enunciado: C17 estricto + POSIX threads.
CC = gcc
CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -Werror -pthread

SRC_DIR = src
INC_DIR = include
BUILD_DIR = build
TARGET = $(BUILD_DIR)/scheduler

# Descubre todos los .c de src/ y deriva su .o correspondiente en build/,
# para no tener que listar cada archivo fuente a mano.
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# Ninguno de estos targets produce un archivo con su propio nombre.
.PHONY: all test test-rng clean

all: $(TARGET)

# Enlaza el binario final a partir de los objetos ya compilados.
$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ $(OBJS)

# Compila cada .c a su .o; el order-only prerequisite (| $(BUILD_DIR))
# asegura que build/ exista sin invalidar el .o si solo cambia el timestamp del directorio.
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Corre las pruebas unitarias de los módulos que ya las tienen. Se va
# extendiendo a medida que existen más (por ahora solo rng).
test: all test-rng

test-rng: $(BUILD_DIR)/test_rng
	./$(BUILD_DIR)/test_rng

$(BUILD_DIR)/test_rng: tests/test_rng.c src/rng.c include/rng.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -o $@ tests/test_rng.c src/rng.c

clean:
	rm -rf $(BUILD_DIR)
