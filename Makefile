# farbherd Makefile
CC ?= cc
CFLAGS ?= -O2
CPPFLAGS += -pedantic -Wall -Wextra

LIBS = libavcodec libavformat libavutil libswscale
LDFLAGS += $(shell pkg-config --libs $(LIBS))
CPPFLAGS += $(shell pkg-config --cflags $(LIBS))

DESTDIR ?= /usr/local

# Don't change after here.
# Or do. I am not your mom.
BINS=2fh fh2blind
DEP=src/farbherd.h

all: $(BINS)

2fh: $(DEP) src/2fh.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/$@.c

fh2blind: $(DEP) src/fh2blind.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ src/$@.c

.PHONY:
install: $(BINS)
	mkdir -p $(DESTDIR)/bin
	install $(BINS) $(DESTDIR)/bin
clean:
	rm -f $(BINS)
