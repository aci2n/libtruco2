CC ?= cc
RM ?= rm -f
MKDIR_P ?= mkdir -p

ENGINE_DIR := ../engine
CPPFLAGS ?= -Iinclude -Isrc -I$(ENGINE_DIR)/include
BASE_CFLAGS := -std=c99 -Wall -Wextra -Wpedantic
DEBUG_CFLAGS := -g -O0 -DDEBUG=1
RELEASE_CFLAGS := -O2
DEBUG ?= 1

ifeq ($(DEBUG),1)
CFLAGS ?= $(BASE_CFLAGS) $(DEBUG_CFLAGS)
else
CFLAGS ?= $(BASE_CFLAGS) $(RELEASE_CFLAGS)
endif

LDFLAGS ?=

BUILD_DIR := build
SERVER_BIN := $(BUILD_DIR)/truco_server
TEST_BIN := $(BUILD_DIR)/test_server
ENGINE_LIB := $(ENGINE_DIR)/lib/libtruco.a

SERVER_SRCS := src/session.c src/protocol.c src/server.c src/main.c
SERVER_OBJS := $(SERVER_SRCS:src/%.c=$(BUILD_DIR)/%.o)
CORE_SRCS := src/session.c src/protocol.c src/server.c
CORE_OBJS := $(CORE_SRCS:src/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean test

all: $(ENGINE_LIB) $(SERVER_BIN)

$(ENGINE_LIB):
	$(MAKE) -C $(ENGINE_DIR)

$(BUILD_DIR)/%.o: src/%.c include/truco_server.h include/truco_server_session.h
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(SERVER_BIN): $(SERVER_OBJS) $(ENGINE_LIB)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(SERVER_OBJS) $(ENGINE_LIB)

$(TEST_BIN): tests/test_server.c $(CORE_OBJS) $(ENGINE_LIB) include/truco_server.h
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_server.c $(CORE_OBJS) $(ENGINE_LIB) -o $@

test: $(SERVER_BIN) $(TEST_BIN)
	./$(TEST_BIN)

clean:
	$(RM) -r $(BUILD_DIR)
