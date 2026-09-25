CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c11
PREFIX ?= /usr/local

.PHONY: all clean install test

all: ugrep

ugrep: ugrep.c
	$(CC) $(CFLAGS) -o $@ $<

install: ugrep
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 ugrep $(DESTDIR)$(PREFIX)/bin/ugrep

clean:
	rm -f ugrep

test: ugrep
	./ugrep -n proc demo.u68
