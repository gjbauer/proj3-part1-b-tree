all:
	clang -o btree btr.c disk.c bitmap.c

clean:
	rm btree *.o

open:
	gedit *.h *.c

.PHONY: clean open
