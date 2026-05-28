CC = gcc
CPPFLAGS = -Iinclude
CFLAGS = -Wall -Werror -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined
LDLIBS = -fsanitize=address,undefined

BUILD_DIR = build
TARGET = $(BUILD_DIR)/swt

SRC = \
	  src/main.c \
	  src/cli/cli.c \
	  src/cli/cmd_run_bugs.c \
	  src/core/defects4j.c \
	  src/core/run_spec.c \
	  src/core/io.c \

OBJ = $(SRC:src/%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ -o $@

$(BUILD_DIR)/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -r $(BUILD_DIR)
