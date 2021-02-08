include config.mk

SOURCE = readopt.c
OBJECT = readopt.o

all: $(STATIC) $(SHARED)

%.o: %.c
	$(CC) -c $(CFLAGS) $<

$(STATIC): $(OBJECT)
	$(AR) $@ $^

$(SHARED): $(OBJECT)
	$(CC) --shared $^ -o $@

install: staticinstall sharedinstall

staticinstall: $(STATIC)
	$(MKDIR) $(DESTDIR)$(LIB)
	$(CP) $^ $(DESTDIR)$(LIB)
	$(MKDIR) $(DESTDIR)$(INCL)
	$(CP) $(HEADER) $(DESTDIR)$(INCL)

sharedinstall: $(SHARED)
	$(MKDIR) $(DESTDIR)$(LIB)
	$(CP) $^ $(DESTDIR)$(LIB)
	$(MKDIR) $(DESTDIR)$(INCL)
	$(CP) $(HEADER) $(DESTDIR)$(INCL)

clean:
	$(RM) $(OBJECT) $(STATIC) $(SHARED)

format:
	clang-format -i -- $(SOURCE) $(HEADER)

.PHONY: clean install staticinstall sharedinstall format
