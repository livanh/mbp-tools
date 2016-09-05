PREFIX=/usr/local

all:
	gcc src/mbp-decode.c -o mbp-decode
	gcc src/mbp-encode.c -o mbp-encode

install:
	install -m 755 mbp-decode $(PREFIX)/bin
	install -m 755 mbp-encode $(PREFIX)/bin

.PHONY: install
