CC = gcc
SANITIZERS = -fsanitize=address,undefined
CFLAGS = -Wall -Werror -Wextra -Wpedantic -g -O0 $(SANITIZERS)

LIBS = libcurl libcjson

PKG_CONFIG ?= pkg-config
DEPS_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(LIBS))
DEPS_LIBS := $(shell $(PKG_CONFIG) --libs $(LIBS))

CPPFLAGS = -Iinclude $(DEPS_CFLAGS)
LDLIBS = $(DEPS_LIBS)
LDFLAGS = $(SANITIZERS)

BUILD_DIR = build
TARGET = $(BUILD_DIR)/swt

SRC = \
	  src/main.c \
	  src/cli/cli.c \
	  src/cli/cmd_run_bugs.c \
	  src/core/defects4j.c \
	  src/core/run_spec.c \
	  src/core/io.c \
	  src/provider/llamacpp.c \

OBJ = $(SRC:src/%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS)  -o $@

$(BUILD_DIR)/%.o: src/%.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
