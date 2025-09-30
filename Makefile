all:
	clang -o btree btr.c disk.c bitmap.c hash.c

clean:
	rm btree

open:
	gedit *.h *.c

.PHONY: clean open
