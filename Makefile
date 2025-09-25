all:
	clang -o btree btr.c

clean:
	rm btree

open:
	gedit *.h *.c

.PHONY: clean open
