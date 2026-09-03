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
.PHONY: all test clean

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

# Placeholder hasta que existan casos de prueba reales.
test: all
	@echo "Sin casos de prueba todavía."

clean:
	rm -rf $(BUILD_DIR)
