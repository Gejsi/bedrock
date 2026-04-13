ifeq ($(origin CC), default)
ifneq ($(shell command -v clang 2>/dev/null),)
CC := clang
else
CC := cc
endif
endif

AR ?= ar
ARFLAGS ?= rcs
MODE ?= debug

SRC_DIR := src
TEST_DIR := tests
BUILD_ROOT := build/$(MODE)
OBJ_DIR := $(BUILD_ROOT)/obj
BIN_DIR := $(BUILD_ROOT)/bin
LIB_DIR := $(BUILD_ROOT)/lib
LIB_TARGET := $(LIB_DIR)/libbedrock.a

CPPFLAGS ?= -Iinclude
BASE_CFLAGS ?= -std=c11
WARN_CFLAGS ?= -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
COMMON_CFLAGS := $(BASE_CFLAGS) $(WARN_CFLAGS)

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

DEBUG_CFLAGS := -O0 -g3 -DDEBUG
RELEASE_CFLAGS := -O3 -DNDEBUG
ASAN_CFLAGS := -O1 -g3 -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer
UBSAN_CFLAGS := -O1 -g3 -DDEBUG -fsanitize=undefined -fno-omit-frame-pointer

DEBUG_LDFLAGS :=
RELEASE_LDFLAGS :=
ASAN_LDFLAGS := -fsanitize=address,undefined
UBSAN_LDFLAGS := -fsanitize=undefined

ifeq ($(MODE),debug)
MODE_CFLAGS := $(DEBUG_CFLAGS)
MODE_LDFLAGS := $(DEBUG_LDFLAGS)
else ifeq ($(MODE),release)
MODE_CFLAGS := $(RELEASE_CFLAGS)
MODE_LDFLAGS := $(RELEASE_LDFLAGS)
else ifeq ($(MODE),asan)
MODE_CFLAGS := $(ASAN_CFLAGS)
MODE_LDFLAGS := $(ASAN_LDFLAGS)
else ifeq ($(MODE),ubsan)
MODE_CFLAGS := $(UBSAN_CFLAGS)
MODE_LDFLAGS := $(UBSAN_LDFLAGS)
else
$(error Unsupported MODE '$(MODE)'; use debug, release, asan, or ubsan)
endif

CFLAGS ?=
CFLAGS += $(COMMON_CFLAGS) $(CLANG_ONLY_CFLAGS) $(MODE_CFLAGS)
LDFLAGS ?=
LDFLAGS += $(MODE_LDFLAGS)

TEST_CFLAGS := $(filter-out -DNDEBUG,$(CFLAGS))
TEST_ENV :=
ifeq ($(MODE),asan)
TEST_ENV += ASAN_OPTIONS=detect_leaks=0
endif
ifeq ($(MODE),ubsan)
TEST_ENV += UBSAN_OPTIONS=print_stacktrace=1
endif

SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' | sort)
TEST_FILES := $(shell find $(TEST_DIR) -type f -name '*.c' | sort)

LIB_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(TEST_FILES))

.PHONY: all clean test check debug release asan ubsan print-config

all: $(LIB_TARGET)

check: test

debug:
	$(MAKE) MODE=debug all

release:
	$(MAKE) MODE=release all

asan:
	$(MAKE) MODE=asan test

ubsan:
	$(MAKE) MODE=ubsan test

print-config:
	@printf 'MODE=%s\n' '$(MODE)'
	@printf 'COMPILER_FAMILY=%s\n' '$(COMPILER_FAMILY)'
	@printf 'CC=%s\n' '$(CC)'
	@printf 'CFLAGS=%s\n' '$(CFLAGS)'
	@printf 'LDFLAGS=%s\n' '$(LDFLAGS)'

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIB_TARGET): $(LIB_OBJS)
	@mkdir -p $(dir $@)
	$(AR) $(ARFLAGS) $@ $(LIB_OBJS)

$(BIN_DIR)/%: $(TEST_DIR)/%.c $(LIB_TARGET)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(TEST_CFLAGS) $< $(LIB_TARGET) $(LDFLAGS) -o $@

test: $(TEST_BINS)
	@set -e; \
	for test_bin in $(TEST_BINS); do \
		printf 'Running %s\n' "$$test_bin"; \
		$(TEST_ENV) "$$test_bin"; \
	done

clean:
	rm -rf build
