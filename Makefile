
.PHONY: install
install:
	mkdir -p ~/.config/kak/autoload/
	for file in "$$(pwd)"/rc/*.kak; do \
	  ln -sf "$${file}" ~/.config/kak/autoload/; \
	done
