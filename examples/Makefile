CC ?= cc
MODE ?= debug
CPPFLAGS ?= -I../include
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic
LDFLAGS ?=
BEDROCK_ROOT ?= ..
BEDROCK_LIB ?= ../build/$(MODE)/lib/libbedrock.a
BUILD_DIR ?= ../build/$(MODE)/examples

ifneq ($(OS),Windows_NT)
CFLAGS += -pthread
LDFLAGS += -pthread
endif

DEPFLAGS := -MMD -MP
SOURCES := $(sort $(wildcard *.c))
BINARIES := $(patsubst %.c,$(BUILD_DIR)/%,$(SOURCES))
DEPS := $(addsuffix .d,$(BINARIES))

.PHONY: all clean

all: $(BINARIES)

$(BEDROCK_LIB):
	$(MAKE) -C $(BEDROCK_ROOT) CC="$(CC)" MODE="$(MODE)" all

$(BUILD_DIR)/%: %.c $(BEDROCK_LIB)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -MF $@.d -MT $@ $< $(BEDROCK_LIB) $(LDFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
