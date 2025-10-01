all:
	clang -g -o btree btr.c disk.c bitmap.c hash.c
	dd if=/dev/zero of=my.img bs=1M count=2

clean:
	rm btree my.img

open:
	gedit *.h *.c

.PHONY: clean open
