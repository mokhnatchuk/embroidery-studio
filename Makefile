CC ?= gcc
FUZZ_CC ?= clang
CLANG_TIDY ?= clang-tidy
PKG_CONFIG ?= pkg-config
PREFIX ?= $(HOME)/.local
FUZZ_TIME ?= 10

TARGET := embroidery-studio
BUILD_DIR := build
RELEASE_OBJ_DIR := $(BUILD_DIR)/release
ASAN_OBJ_DIR := $(BUILD_DIR)/asan
FUZZ_DIR := $(BUILD_DIR)/fuzz
TEST_BIN := $(BUILD_DIR)/tests
BENCH_BIN := $(BUILD_DIR)/bench

WARN_CFLAGS := -std=c23 -Wall -Wextra -Wpedantic -Werror -Wvla -Wshadow -Wformat=2 -Isrc
RELEASE_CFLAGS := $(WARN_CFLAGS) -O3
SANITIZER_CFLAGS := $(WARN_CFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all
FUZZ_CFLAGS := $(WARN_CFLAGS) -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all -DEMBROIDERY_QUIET_ERRORS

SDL3_CFLAGS := $(shell $(PKG_CONFIG) --cflags sdl3)
SDL3_LIBS := $(shell $(PKG_CONFIG) --libs sdl3)
FREETYPE_CFLAGS := $(shell $(PKG_CONFIG) --cflags freetype2)
FREETYPE_LIBS := $(shell $(PKG_CONFIG) --libs freetype2)

CORE_MODULES := model formats ops render ui
RELEASE_CORE_OBJS := $(patsubst %,$(RELEASE_OBJ_DIR)/%.o,$(CORE_MODULES))
RELEASE_MAIN_OBJ := $(RELEASE_OBJ_DIR)/main.o
ASAN_CORE_OBJS := $(patsubst %,$(ASAN_OBJ_DIR)/%.o,$(CORE_MODULES))
FUZZ_BINS := $(FUZZ_DIR)/fuzz_jef $(FUZZ_DIR)/fuzz_pes $(FUZZ_DIR)/fuzz_pec $(FUZZ_DIR)/fuzz_dst

ASAN_ENV := ASAN_OPTIONS=detect_leaks=1:strict_string_checks=1:detect_stack_use_after_return=1 \
            UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1

.PHONY: all test bench lint fuzz install uninstall clean

all: $(TARGET)

$(RELEASE_OBJ_DIR) $(ASAN_OBJ_DIR) $(FUZZ_DIR):
	mkdir -p $@

$(RELEASE_OBJ_DIR)/model.o: src/model.c src/model.h | $(RELEASE_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

$(RELEASE_OBJ_DIR)/formats.o: src/formats.c src/formats.h src/model.h | $(RELEASE_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

$(RELEASE_OBJ_DIR)/ops.o: src/ops.c src/ops.h src/model.h | $(RELEASE_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

$(RELEASE_OBJ_DIR)/render.o: src/render.c src/render.h src/ops.h src/model.h | $(RELEASE_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) -c $< -o $@

$(RELEASE_OBJ_DIR)/ui.o: src/ui.c src/ui.h src/render.h src/ops.h src/model.h | $(RELEASE_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) $(FREETYPE_CFLAGS) -c $< -o $@

$(RELEASE_MAIN_OBJ): src/main.c src/ui.h src/render.h src/ops.h src/formats.h src/model.h | $(RELEASE_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) $(SDL3_CFLAGS) -c $< -o $@

$(TARGET): $(RELEASE_CORE_OBJS) $(RELEASE_MAIN_OBJ)
	$(CC) $(RELEASE_CFLAGS) $^ -o $@ $(SDL3_LIBS) $(FREETYPE_LIBS) -lm

$(ASAN_OBJ_DIR)/model.o: src/model.c src/model.h | $(ASAN_OBJ_DIR)
	$(CC) $(SANITIZER_CFLAGS) -c $< -o $@

$(ASAN_OBJ_DIR)/formats.o: src/formats.c src/formats.h src/model.h | $(ASAN_OBJ_DIR)
	$(CC) $(SANITIZER_CFLAGS) -c $< -o $@

$(ASAN_OBJ_DIR)/ops.o: src/ops.c src/ops.h src/model.h | $(ASAN_OBJ_DIR)
	$(CC) $(SANITIZER_CFLAGS) -c $< -o $@

$(ASAN_OBJ_DIR)/render.o: src/render.c src/render.h src/ops.h src/model.h | $(ASAN_OBJ_DIR)
	$(CC) $(SANITIZER_CFLAGS) -c $< -o $@

$(ASAN_OBJ_DIR)/ui.o: src/ui.c src/ui.h src/render.h src/ops.h src/model.h | $(ASAN_OBJ_DIR)
	$(CC) $(SANITIZER_CFLAGS) $(FREETYPE_CFLAGS) -c $< -o $@

$(TEST_BIN): tests/tests.c $(ASAN_CORE_OBJS) src/model.h src/formats.h src/ops.h src/render.h src/ui.h | $(ASAN_OBJ_DIR)
	$(CC) $(SANITIZER_CFLAGS) tests/tests.c $(ASAN_CORE_OBJS) -o $@ $(FREETYPE_LIBS) -lm

test: $(TEST_BIN)
	$(ASAN_ENV) ./$(TEST_BIN)

$(BENCH_BIN): tests/bench.c $(RELEASE_CORE_OBJS) src/model.h src/formats.h src/ops.h src/render.h src/ui.h | $(RELEASE_OBJ_DIR)
	$(CC) $(RELEASE_CFLAGS) tests/bench.c $(RELEASE_CORE_OBJS) -o $@ -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc $(FREETYPE_LIBS) -lm

bench: $(BENCH_BIN)
	./$(BENCH_BIN)

lint:
	$(CLANG_TIDY) src/model.c src/formats.c src/ops.c src/render.c -- -std=c23 -Isrc
	$(CLANG_TIDY) src/ui.c -- -std=c23 -Isrc $(FREETYPE_CFLAGS)
	$(CLANG_TIDY) src/main.c -- -std=c23 -Isrc $(SDL3_CFLAGS)

$(FUZZ_DIR)/fuzz_jef: tests/fuzz_formats.c src/model.c src/formats.c src/model.h src/formats.h | $(FUZZ_DIR)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -DFUZZ_TARGET_JEF tests/fuzz_formats.c src/model.c src/formats.c -o $@ -lm

$(FUZZ_DIR)/fuzz_pes: tests/fuzz_formats.c src/model.c src/formats.c src/model.h src/formats.h | $(FUZZ_DIR)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -DFUZZ_TARGET_PES tests/fuzz_formats.c src/model.c src/formats.c -o $@ -lm

$(FUZZ_DIR)/fuzz_pec: tests/fuzz_formats.c src/model.c src/formats.c src/model.h src/formats.h | $(FUZZ_DIR)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -DFUZZ_TARGET_PEC tests/fuzz_formats.c src/model.c src/formats.c -o $@ -lm

$(FUZZ_DIR)/fuzz_dst: tests/fuzz_formats.c src/model.c src/formats.c src/model.h src/formats.h | $(FUZZ_DIR)
	$(FUZZ_CC) $(FUZZ_CFLAGS) -DFUZZ_TARGET_DST tests/fuzz_formats.c src/model.c src/formats.c -o $@ -lm

fuzz: $(FUZZ_BINS)
	mkdir -p $(FUZZ_DIR)/corpus_jef $(FUZZ_DIR)/corpus_pes $(FUZZ_DIR)/corpus_pec $(FUZZ_DIR)/corpus_dst
	$(ASAN_ENV) ./$(FUZZ_DIR)/fuzz_jef $(FUZZ_DIR)/corpus_jef -max_total_time=$(FUZZ_TIME) -max_len=65536
	$(ASAN_ENV) ./$(FUZZ_DIR)/fuzz_pes $(FUZZ_DIR)/corpus_pes -max_total_time=$(FUZZ_TIME) -max_len=65536
	$(ASAN_ENV) ./$(FUZZ_DIR)/fuzz_pec $(FUZZ_DIR)/corpus_pec -max_total_time=$(FUZZ_TIME) -max_len=65536
	$(ASAN_ENV) ./$(FUZZ_DIR)/fuzz_dst $(FUZZ_DIR)/corpus_dst -max_total_time=$(FUZZ_TIME) -max_len=65536

install: all
	install -Dm755 $(TARGET) $(PREFIX)/bin/$(TARGET)
	install -Dm644 assets/embroidery-studio.desktop $(PREFIX)/share/applications/embroidery-studio.desktop

uninstall:
	rm -f $(PREFIX)/bin/$(TARGET)
	rm -f $(PREFIX)/share/applications/embroidery-studio.desktop

clean:
	rm -rf $(TARGET) $(BUILD_DIR)
