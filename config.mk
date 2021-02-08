DESTDIR =
PREFIX  = /usr/local/
LIB     = $(PREFIX)/lib/
INCL    = $(PREFIX)/include/

CC      = cc
MACROS  = -D _POSIX_C_SOURCE=200809L -D NDEBUG
CFLAGS  = --std=c99 -Wall -Wextra -Wpedantic -g $(MACROS) -O2

AR      = ar -rcs --
RM      = rm -f --
CP      = cp --
MKDIR   = mkdir -p --

HEADER  = ./readopt.h
STATIC  = ./libreadopt.a
SHARED  = ./libreadopt.so
