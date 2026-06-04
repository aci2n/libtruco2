CC ?= cc
AR ?= ar
RM ?= rm -f
MKDIR_P ?= mkdir -p

PREFIX ?= /usr/local
DESTDIR ?=
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include

CPPFLAGS ?= -Iinclude

BASE_CFLAGS := -std=c99 -Wall -Wextra -Wpedantic -fPIC
DEBUG_CFLAGS := -g -O0 -DDEBUG=1
RELEASE_CFLAGS := -O2

DEBUG ?= 1

ifeq ($(DEBUG),1)
CFLAGS ?= $(BASE_CFLAGS) $(DEBUG_CFLAGS)
else
CFLAGS ?= $(BASE_CFLAGS) $(RELEASE_CFLAGS)
endif

LDFLAGS ?=

LIB_NAME := truco
BUILD_DIR := build
LIB_DIR := lib
STATIC_LIB := $(LIB_DIR)/lib$(LIB_NAME).a
SHARED_LIB := $(LIB_DIR)/lib$(LIB_NAME).so
SRC := src/truco_cards.c src/truco_event.c src/truco_hand.c src/truco_game.c
OBJ := $(BUILD_DIR)/truco_cards.o $(BUILD_DIR)/truco_event.o $(BUILD_DIR)/truco_hand.o $(BUILD_DIR)/truco_game.o
TEST_BIN := $(BUILD_DIR)/test_truco
EXAMPLE_BIN := $(BUILD_DIR)/basic_round

.PHONY: all clean test install uninstall examples

all: $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR)/truco_cards.o: src/truco_cards.c include/truco.h include/truco_internal.h
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/truco_cards.c -o $@

$(BUILD_DIR)/truco_event.o: src/truco_event.c src/truco_hand_internal.h include/truco.h include/truco_internal.h
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/truco_event.c -o $@

$(BUILD_DIR)/truco_hand.o: src/truco_hand.c src/truco_hand_internal.h include/truco.h include/truco_internal.h
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/truco_hand.c -o $@

$(BUILD_DIR)/truco_game.o: src/truco_game.c src/truco_hand_internal.h include/truco.h include/truco_internal.h
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/truco_game.c -o $@

$(STATIC_LIB): $(OBJ)
	$(MKDIR_P) $(LIB_DIR)
	$(AR) rcs $@ $^

$(SHARED_LIB): $(OBJ)
	$(MKDIR_P) $(LIB_DIR)
	$(CC) -shared $(LDFLAGS) -o $@ $^

$(TEST_BIN): tests/test_truco.c $(STATIC_LIB) include/truco.h
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_truco.c $(STATIC_LIB) -o $@

$(EXAMPLE_BIN): examples/basic_round.c $(STATIC_LIB) include/truco.h
	$(MKDIR_P) $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) examples/basic_round.c $(STATIC_LIB) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

examples: $(EXAMPLE_BIN)

install: all
	$(MKDIR_P) "$(DESTDIR)$(LIBDIR)" "$(DESTDIR)$(INCLUDEDIR)"
	cp $(STATIC_LIB) "$(DESTDIR)$(LIBDIR)/"
	cp $(SHARED_LIB) "$(DESTDIR)$(LIBDIR)/"
	cp include/truco.h "$(DESTDIR)$(INCLUDEDIR)/"

uninstall:
	$(RM) "$(DESTDIR)$(LIBDIR)/lib$(LIB_NAME).a"
	$(RM) "$(DESTDIR)$(LIBDIR)/lib$(LIB_NAME).so"
	$(RM) "$(DESTDIR)$(INCLUDEDIR)/truco.h"

clean:
	$(RM) -r $(BUILD_DIR) $(LIB_DIR)
