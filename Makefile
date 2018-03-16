prefix=/usr/local

all:
	gcc src/mbp-decode.c -o mbp-decode
	gcc src/mbp-encode.c -o mbp-encode

install:
	mkdir -p $(DESTDIR)$(prefix)/bin
	install -m 755 mbp-decode $(DESTDIR)$(prefix)/bin
	install -m 755 mbp-encode $(DESTDIR)$(prefix)/bin

.PHONY: install

