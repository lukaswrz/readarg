CC = cc
LDFLAGS = -lreadopt
MACROS = -D_POSIX_C_SOURCE=200809L
CFLAGS = $(MACROS) --std=c99 -O2 -g -Wall -Wextra -Wpedantic

SOURCES = test.c
OBJECTS = $(SOURCES:.c=.o)

RM = rm -f --
CP = cp --

all: test

%.o: %.c
	$(CC) $(CFLAGS) -c $<

test: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(OBJECTS)

.PHONY: clean
