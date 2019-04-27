# farbherd Makefile
CC ?= cc
CFLAGS ?= -O2 -lm
CPPFLAGS += -pedantic -Wall -Wextra
CPPFLAGS += -I../farbherd/src

LIBS = libavcodec libavformat libavutil libswscale
LDFLAGS += $(shell pkg-config --libs $(LIBS))
CPPFLAGS += $(shell pkg-config --cflags $(LIBS))

PREFIX ?= /usr/local
DESTIR ?= /

# Don't change after here.
# Or do. I am not your mom.
BINS=2fh fh2blind blind2fh
DEP=src/conversion.h

all: $(BINS)

2fh: $(DEP) src/2fh.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/$@.c

blind2fh: $(DEP) src/blind2fh.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/$@.c
fh2blind: $(DEP) src/fh2blind.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/$@.c

.PHONY:
install: $(BINS)
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	install $(BINS) $(DESTDIR)/$(PREFIX)/bin

uninstall:
	cd $(DESTDIR)/$(PREFIX)/bin && rm -f $(BINS)


clean:
	rm -f $(BINS)
