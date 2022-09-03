prefix	= /usr/local
ifeq ($(OS),Windows_NT)
	CC=gcc
	LIBS=-lws2_32
	prefix="C:\\Program Files\\rep"
endif

all: rep test rep.1

rep: rep.c
	$(CC) -g -O2 -o rep rep.c $(LIBS)


rep.1: rep.1.adoc
	a2x -f manpage rep.1.adoc

test:
	:

.PHONY: install
install:
	mkdir -p $(prefix)/bin/ $(prefix)/share/man/man1/ $(prefix)/share/kak/autoload/plugins/
	cp rep $(prefix)/bin/
	cp rep.1 $(prefix)/share/man/man1/
	cp rc/rep.kak $(prefix)/share/kak/autoload/plugins/rep.kak
