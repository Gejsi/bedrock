ifeq ($(origin CC), default)
ifneq ($(shell command -v clang 2>/dev/null),)
CC := clang
else
CC := cc
endif
endif

AR ?= ar
ARFLAGS ?= rcs
CLANG_FORMAT ?= clang-format
MODE ?= debug

SRC_DIR := src
TEST_DIR := tests
FORMAT_DIRS := include src tests
BUILD_ROOT := build/$(MODE)
OBJ_DIR := $(BUILD_ROOT)/obj
BIN_DIR := $(BUILD_ROOT)/bin
LIB_DIR := $(BUILD_ROOT)/lib
LIB_TARGET := $(LIB_DIR)/libbedrock.a

CPPFLAGS ?= -Iinclude
BASE_CFLAGS ?= -std=c11
WARN_CFLAGS ?= -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
COMMON_CFLAGS := $(BASE_CFLAGS) $(WARN_CFLAGS)

ifeq ($(OS),Windows_NT)
THREAD_CFLAGS :=
THREAD_LDFLAGS :=
THREAD_CPPFLAGS :=
else
THREAD_CFLAGS := -pthread
THREAD_LDFLAGS := -pthread
THREAD_CPPFLAGS := -D_XOPEN_SOURCE=700
endif

CPPFLAGS += $(THREAD_CPPFLAGS)

CC_VERSION := $(shell $(CC) --version 2>/dev/null | head -n 1)
ifneq ($(findstring clang,$(CC_VERSION)),)
COMPILER_FAMILY := clang
else
COMPILER_FAMILY := gcc
endif

CLANG_ONLY_CFLAGS :=
ifeq ($(COMPILER_FAMILY),clang)
CLANG_ONLY_CFLAGS += -Wnewline-eof -Wno-unsafe-buffer-usage
endif

DEPFLAGS := -MMD -MP

DEBUG_CFLAGS := -O0 -g3 -DDEBUG -fno-omit-frame-pointer -fno-optimize-sibling-calls
RELEASE_CFLAGS := -O3 -DNDEBUG
SANITIZE_CFLAGS := -O1 -g3 -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer -fno-optimize-sibling-calls

DEBUG_LDFLAGS :=
RELEASE_LDFLAGS :=
SANITIZE_LDFLAGS := -fsanitize=address,undefined

ifeq ($(MODE),debug)
MODE_CFLAGS := $(DEBUG_CFLAGS)
MODE_LDFLAGS := $(DEBUG_LDFLAGS)
else ifeq ($(MODE),release)
MODE_CFLAGS := $(RELEASE_CFLAGS)
MODE_LDFLAGS := $(RELEASE_LDFLAGS)
else ifeq ($(MODE),sanitize)
MODE_CFLAGS := $(SANITIZE_CFLAGS)
MODE_LDFLAGS := $(SANITIZE_LDFLAGS)
else ifeq ($(MODE),asan)
MODE_CFLAGS := $(SANITIZE_CFLAGS)
MODE_LDFLAGS := $(SANITIZE_LDFLAGS)
else
$(error Unsupported MODE '$(MODE)'; use debug, release, or sanitize)
endif

CFLAGS ?=
CFLAGS += $(COMMON_CFLAGS) $(CLANG_ONLY_CFLAGS) $(MODE_CFLAGS) $(THREAD_CFLAGS)
LDFLAGS ?=
LDFLAGS += $(MODE_LDFLAGS) $(THREAD_LDFLAGS)

TEST_CFLAGS := $(filter-out -DNDEBUG,$(CFLAGS))
TEST_ENV :=
ifeq ($(MODE),sanitize)
TEST_ENV += ASAN_OPTIONS=detect_leaks=0
endif
ifeq ($(MODE),asan)
TEST_ENV += ASAN_OPTIONS=detect_leaks=0
endif

SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' | sort)
TEST_FILES := $(shell find $(TEST_DIR) -type f -name '*.c' | sort)
FORMAT_FILES := $(shell find $(FORMAT_DIRS) -type f \( -name '*.c' -o -name '*.h' \) | sort)

LIB_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(TEST_FILES))
LIB_DEPS := $(LIB_OBJS:.o=.d)
TEST_DEPS := $(addsuffix .d,$(TEST_BINS))

.PHONY: all clean test check debug release sanitize asan format check-format print-config

all: $(LIB_TARGET)

check: test

debug:
	$(MAKE) MODE=debug all

release:
	$(MAKE) MODE=release all

sanitize:
	$(MAKE) MODE=sanitize test

asan:
	$(MAKE) MODE=sanitize test

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

check-format:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

print-config:
	@printf 'MODE=%s\n' '$(MODE)'
	@printf 'COMPILER_FAMILY=%s\n' '$(COMPILER_FAMILY)'
	@printf 'CC=%s\n' '$(CC)'
	@printf 'CFLAGS=%s\n' '$(CFLAGS)'
	@printf 'LDFLAGS=%s\n' '$(LDFLAGS)'

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -MF $(@:.o=.d) -MT $@ -c $< -o $@

$(LIB_TARGET): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	rm -f $@
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

$(BIN_DIR)/%: $(TEST_DIR)/%.c $(LIB_TARGET)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $(DEPFLAGS) -MF $@.d -MT $@ $< $(LIB_TARGET) $(LDFLAGS) -o $@

test: $(TEST_BINS)
	@set -e; \
	for test_bin in $(TEST_BINS); do \
		printf 'Running %s\n' "$$test_bin"; \
		$(TEST_ENV) "$$test_bin"; \
	done

clean:
	rm -rf build

-include $(LIB_DEPS) $(TEST_DEPS)
