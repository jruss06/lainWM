PREFIX=/usr/local
libs=-lxcb -lxcb-randr -lxcb-keysyms -lxcb-icccm -lxcb-ewmh

lainwm: lainwm.c
	gcc lainwm.c -o lainwm $(libs) 

install: lainwm
	cp $< $(PREFIX)/bin
