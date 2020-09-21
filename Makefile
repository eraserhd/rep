
all: rep test rep.1

rep: rep.c
	$(CC) -g -O2 -o rep rep.c

rep.1: rep.1.adoc
	a2x -f manpage rep.1.adoc

test:
	:

.PHONY: install
install:
	mkdir -p ~/.config/kak/autoload/
	for file in "$$(pwd)"/rc/*.kak; do \
	  ln -sf "$${file}" ~/.config/kak/autoload/; \
	done
