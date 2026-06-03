CC ?= cc
AR ?= ar
RM ?= rm -f
MKDIR_P ?= mkdir -p

PREFIX ?= /usr/local
DESTDIR ?=
LIBDIR ?= $(PREFIX)/lib
INCLUDEDIR ?= $(PREFIX)/include

CPPFLAGS ?= -Iinclude
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -O2 -fPIC
LDFLAGS ?=

LIB_NAME := truco
STATIC_LIB := lib/lib$(LIB_NAME).a
SHARED_LIB := lib/lib$(LIB_NAME).so
SRC := src/truco.c
OBJ := build/truco.o
TEST_BIN := build/test_truco
EXAMPLE_BIN := build/basic_round

.PHONY: all clean test install uninstall examples

all: $(STATIC_LIB) $(SHARED_LIB)

$(OBJ): $(SRC) include/truco.h src/truco_internal.h
	$(MKDIR_P) build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(STATIC_LIB): $(OBJ)
	$(MKDIR_P) lib
	$(AR) rcs $@ $^

$(SHARED_LIB): $(OBJ)
	$(MKDIR_P) lib
	$(CC) -shared $(LDFLAGS) -o $@ $^

$(TEST_BIN): tests/test_truco.c $(STATIC_LIB) include/truco.h
	$(MKDIR_P) build
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_truco.c $(STATIC_LIB) -o $@

$(EXAMPLE_BIN): examples/basic_round.c $(STATIC_LIB) include/truco.h
	$(MKDIR_P) build
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
	$(RM) -r build lib
