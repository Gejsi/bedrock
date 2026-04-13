CC ?= cc
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Wpedantic -Iinclude

BUILD_DIR := build
SRC_DIR := src
TEST_DIR := tests

LIB_OBJS := \
	$(BUILD_DIR)/alloc.o \
	$(BUILD_DIR)/arena.o

.PHONY: all clean test

all: $(BUILD_DIR)/libbedrock.a

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/alloc.o: $(SRC_DIR)/alloc.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/arena.o: $(SRC_DIR)/arena.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/libbedrock.a: $(LIB_OBJS)
	$(AR) rcs $@ $(LIB_OBJS)

$(BUILD_DIR)/test_bootstrap: $(TEST_DIR)/test_bootstrap.c $(BUILD_DIR)/libbedrock.a
	$(CC) $(CFLAGS) $< $(BUILD_DIR)/libbedrock.a -o $@

test: $(BUILD_DIR)/test_bootstrap
	$(BUILD_DIR)/test_bootstrap

clean:
	rm -rf $(BUILD_DIR)
