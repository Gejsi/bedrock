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

PYTHON ?= python3
AMALGAMATE := tools/amalgamate.py
DIST_DIR := dist
DIST_HEADER := $(DIST_DIR)/bedrock.h
DIST_SOURCE := $(DIST_DIR)/bedrock.c
# Warning set for the dist smoke compiles: strict, standalone (no repo CFLAGS,
# since consumers build the amalgamation with their own flags).
DIST_SMOKE_CFLAGS := -std=c11 -Wall -Wextra -Werror

CPPFLAGS ?= -Iinclude
BASE_CFLAGS ?= -std=c11
WARN_CFLAGS ?= -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wstrict-prototypes
COMMON_CFLAGS := $(BASE_CFLAGS) $(WARN_CFLAGS)

ifeq ($(OS),Windows_NT)
THREAD_CFLAGS :=
THREAD_LDFLAGS :=
else
THREAD_CFLAGS := -pthread
THREAD_LDFLAGS := -pthread
endif

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
THREAD_SANITIZE_CFLAGS := -O1 -g3 -DDEBUG -fsanitize=thread -fno-omit-frame-pointer -fno-optimize-sibling-calls

DEBUG_LDFLAGS :=
RELEASE_LDFLAGS :=
SANITIZE_LDFLAGS := -fsanitize=address,undefined
THREAD_SANITIZE_LDFLAGS := -fsanitize=thread

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
else ifeq ($(MODE),thread-sanitize)
MODE_CFLAGS := $(THREAD_SANITIZE_CFLAGS)
MODE_LDFLAGS := $(THREAD_SANITIZE_LDFLAGS)
else ifeq ($(MODE),tsan)
MODE_CFLAGS := $(THREAD_SANITIZE_CFLAGS)
MODE_LDFLAGS := $(THREAD_SANITIZE_LDFLAGS)
else
$(error Unsupported MODE '$(MODE)'; use debug, release, sanitize, or thread-sanitize)
endif

CFLAGS ?=
CFLAGS += $(COMMON_CFLAGS) $(CLANG_ONLY_CFLAGS) $(MODE_CFLAGS) $(THREAD_CFLAGS)
LDFLAGS ?=
LDFLAGS += $(MODE_LDFLAGS) $(THREAD_LDFLAGS)

TEST_CFLAGS := $(filter-out -DNDEBUG,$(CFLAGS))
# Leak detection (LSan) runs by default with ASan on Linux. Apple clang does
# not support it, so macOS sanitize runs are unaffected. Do not disable it
# globally; scope any intentional-leak test with LSAN_OPTIONS suppressions.
TEST_ENV :=

SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' | sort)
TEST_FILES := $(shell find $(TEST_DIR) -type f -name '*.c' | sort)
FORMAT_FILES := $(shell find $(FORMAT_DIRS) -type f \( -name '*.c' -o -name '*.h' \) | sort)

LIB_OBJS := $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TEST_BINS := $(patsubst $(TEST_DIR)/%.c,$(BIN_DIR)/%,$(TEST_FILES))
LIB_DEPS := $(LIB_OBJS:.o=.d)
TEST_DEPS := $(addsuffix .d,$(TEST_BINS))

.PHONY: all clean test check debug release sanitize asan thread-sanitize tsan format check-format print-config dist check-dist dist-smoke

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

thread-sanitize:
	$(MAKE) MODE=thread-sanitize test

tsan:
	$(MAKE) MODE=thread-sanitize test

format:
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

check-format:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

# Regenerate the single-header distribution from the modular tree.
dist:
	$(PYTHON) $(AMALGAMATE)

# Fail if the committed dist/ is stale. Regeneration is idempotent, so after
# `make dist` any change to dist/ means the tree drifted from include/+src/;
# `git diff --exit-code` reports it. This is the CI gate that forces `make dist`
# after any header or source change.
check-dist: dist
	@if git diff --quiet -- $(DIST_DIR); then \
		printf 'dist/ is up to date.\n'; \
	else \
		printf 'error: dist/ is stale; run `make dist` and commit the result.\n' >&2; \
		git --no-pager diff --stat -- $(DIST_DIR) >&2; \
		exit 1; \
	fi

# Compile the amalgamation in every supported consumption mode with -Werror.
# Driver TUs are generated under build/ (never tests/, which is auto-discovered
# and compiled against -Iinclude where dist/bedrock.h does not exist).
dist-smoke: dist
	@set -e; \
	smoke=build/dist-smoke; \
	rm -rf "$$smoke"; mkdir -p "$$smoke"; \
	cp $(DIST_HEADER) $(DIST_SOURCE) "$$smoke/"; \
	printf '#include "bedrock.h"\nint bedrock_smoke_use(void) {\n  br_arena a; unsigned char b[64]; br_alloc_result r;\n  br_arena_init(&a, b, sizeof b);\n  r = br_arena_alloc(&a, 8u, 8u);\n  return r.status == BR_STATUS_OK ? 0 : 1;\n}\nint main(void) { return bedrock_smoke_use(); }\n' > "$$smoke/use.c"; \
	printf '#include "bedrock.h"\nint bedrock_smoke_second(void) { return (int)sizeof(br_arena); }\n' > "$$smoke/second.c"; \
	printf '#define BEDROCK_IMPLEMENTATION\n#include "bedrock.h"\nint main(void) {\n  br_arena a; unsigned char b[64]; br_alloc_result r;\n  br_arena_init(&a, b, sizeof b);\n  r = br_arena_alloc(&a, 8u, 8u);\n  return r.status == BR_STATUS_OK ? 0 : 1;\n}\n' > "$$smoke/single.c"; \
	printf '#define BEDROCK_NO_SHORT_TYPES\n#include "bedrock.h"\nint main(void) { return (int)sizeof(br_alloc_result) > 0 ? 0 : 1; }\n' > "$$smoke/noshort.c"; \
	printf 'dist-smoke: two-file mode\n'; \
	$(CC) $(DIST_SMOKE_CFLAGS) -I"$$smoke" "$$smoke/bedrock.c" "$$smoke/use.c" $(THREAD_CFLAGS) $(THREAD_LDFLAGS) -o "$$smoke/two_file"; \
	"$$smoke/two_file"; \
	printf 'dist-smoke: single-header mode\n'; \
	$(CC) $(DIST_SMOKE_CFLAGS) -I"$$smoke" "$$smoke/single.c" $(THREAD_CFLAGS) $(THREAD_LDFLAGS) -o "$$smoke/single"; \
	"$$smoke/single"; \
	printf 'dist-smoke: declarations with -DBEDROCK_NO_SHORT_TYPES\n'; \
	$(CC) $(DIST_SMOKE_CFLAGS) -I"$$smoke" "$$smoke/noshort.c" $(THREAD_CFLAGS) $(THREAD_LDFLAGS) -o "$$smoke/noshort"; \
	"$$smoke/noshort"; \
	printf 'dist-smoke: two-TU link (no duplicate symbols)\n'; \
	$(CC) $(DIST_SMOKE_CFLAGS) -I"$$smoke" "$$smoke/bedrock.c" "$$smoke/use.c" "$$smoke/second.c" $(THREAD_CFLAGS) $(THREAD_LDFLAGS) -o "$$smoke/two_tu"; \
	"$$smoke/two_tu"; \
	printf 'dist-smoke: all modes passed.\n'

print-config:
	@printf 'MODE=%s\n' '$(MODE)'
	@printf 'COMPILER_FAMILY=%s\n' '$(COMPILER_FAMILY)'
	@printf 'CC=%s\n' '$(CC)'
	@printf 'CPPFLAGS=%s\n' '$(CPPFLAGS)'
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

# The no-short-types smoke test must build with the short aliases disabled so it
# proves the public ABI is spelled entirely in standard C types. The define is
# hardcoded in this dedicated recipe rather than set as a target-specific
# variable: target-specific variables propagate to prerequisites, which would
# leak -DBEDROCK_NO_SHORT_TYPES into the library objects built for this target.
$(BIN_DIR)/test_no_short_types: $(TEST_DIR)/test_no_short_types.c $(LIB_TARGET)
	@mkdir -p $(dir $@)
	$(CC) -DBEDROCK_NO_SHORT_TYPES $(CPPFLAGS) $(TEST_CFLAGS) $(DEPFLAGS) -MF $@.d -MT $@ $< $(LIB_TARGET) $(LDFLAGS) -o $@

test: $(TEST_BINS)
	@set -e; \
	for test_bin in $(TEST_BINS); do \
		printf 'Running %s\n' "$$test_bin"; \
		$(TEST_ENV) "$$test_bin"; \
	done

clean:
	rm -rf build

-include $(LIB_DEPS) $(TEST_DEPS)
