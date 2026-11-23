CC ?= gcc
PKG_CONFIG ?= pkg-config
PREFIX ?= $(HOME)/.local

TARGET := embroidery-studio
BUILD_DIR := build
TEST_BIN := $(BUILD_DIR)/tests

COMMON_CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -Werror -Isrc
RELEASE_CFLAGS := $(COMMON_CFLAGS) -O3
SANITIZER_CFLAGS := $(COMMON_CFLAGS) -O1 -g -fsanitize=address,undefined

SDL2_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl2)
SDL2_LIBS := $(shell $(PKG_CONFIG) --libs sdl2)

.PHONY: all test install uninstall clean

all: $(TARGET)

$(TARGET): src/main.c src/core.h
	$(CC) $(RELEASE_CFLAGS) $(SDL2_CFLAGS) src/main.c -o $(TARGET) $(SDL2_LIBS) -lm

$(TEST_BIN): tests/tests.c src/core.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(SANITIZER_CFLAGS) tests/tests.c -o $(TEST_BIN) -lm

test: $(TEST_BIN)
	./$(TEST_BIN)

install: all
	install -Dm755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	install -Dm644 assets/embroidery-studio.desktop $(PREFIX)/share/applications/embroidery-studio.desktop

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -f $(PREFIX)/share/applications/embroidery-studio.desktop

clean:
	rm -rf $(TARGET) $(BUILD_DIR)
