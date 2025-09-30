#include "bitmap.h"

int bitmap_get(void* bm, int ii) {
	uint64_t* ptr = (uint64_t*)bm;
	ptr = ptr + ( ii / 64 );
	return (*ptr & (1 << (ii % 64))) >> (ii % 64);
}

void bitmap_put(void* bm, int ii, int vv) {
	uint64_t* ptr = (uint64_t*)bm;
	ptr = ptr + ( ii / 64 );
	*ptr |= (1 << (ii % 64));
}

void bitmap_print(void* bm, int size) {
	return;
}

