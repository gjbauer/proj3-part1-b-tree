all:
	clang -o btree-test btr.c

clean:
	rm btree

open:
	gedit *.h *.c

.PHONY: clean open
