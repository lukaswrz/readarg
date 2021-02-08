CC = cc
MACROS = -DNDEBUG
CFLAGS = $(MACROS) --std=c99 -O2 -g -Wall -Wextra -Wpedantic -fPIC

PREFIX = /usr/local/
LIB = $(PREFIX)/lib/
INCLUDE = $(PREFIX)/include/

SOURCES = $(wildcard ./*.c)
OBJECTS = $(SOURCES:.c=.o)
HEADERS = $(wildcard ./*.h)

STATICTARGET = libreadopt.a
SHAREDTARGET = libreadopt.so

AR = ar -rcs --
RM = rm -f --
CP = cp --
MKDIR = mkdir -p --

all: $(STATICTARGET) $(SHAREDTARGET)

%.o: %.c
	$(CC) -c $(CFLAGS) $<

$(STATICTARGET): $(OBJECTS)
	$(AR) $@ $^

$(SHAREDTARGET): $(OBJECTS)
	$(CC) --shared $^ -o $@

install: staticinstall sharedinstall

staticinstall: $(STATICTARGET)
	$(MKDIR) $(DESTDIR)$(LIB)
	$(CP) $^ $(DESTDIR)$(LIB)
	$(MKDIR) $(DESTDIR)$(INCLUDE)
	$(CP) $(HEADERS) $(DESTDIR)$(INCLUDE)

sharedinstall: $(SHAREDTARGET)
	$(MKDIR) $(DESTDIR)$(LIB)
	$(CP) $^ $(DESTDIR)$(LIB)
	$(MKDIR) $(DESTDIR)$(INCLUDE)
	$(CP) $(HEADERS) $(DESTDIR)$(INCLUDE)

clean:
	$(RM) $(OBJECTS) $(STATICTARGET) $(SHAREDTARGET)

format:
	clang-format -i -- $(SOURCES) $(HEADERS)

.PHONY: clean install staticinstall sharedinstall format
