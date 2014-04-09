PREFIX ?= /usr/local

all: zamcomp.so

zamcomp.so: zamcomp.c Makefile
	gcc -std=gnu99 -O3 -ffast-math -fPIC -DPIC -c zamcomp.c
	ld -shared -soname zamcomp.so -o zamcomp.so -lc zamcomp.o

install:
	install -d $(DESTDIR)$(PREFIX)/lib/ladspa
	install zamcomp.so -t $(DESTDIR)$(PREFIX)/lib/ladspa

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/lib/ladspa/zamcomp.so

clean:
	rm -f zamcomp.so zamcomp.o
