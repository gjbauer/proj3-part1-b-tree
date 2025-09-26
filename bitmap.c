#include "bitmap.h"

// TODO: Make the bitmap index to only specific bits in an int ans not the whole int, gives us many times more space per int to store data

int bitmap_get(void* bm, int ii) {
	int* ptr = (int*)bm;
	ptr =  ptr + ii;
	return *ptr;
}

void bitmap_put(void* bm, int ii, int vv) {
	int* ptr = (int*)bm;
	ptr =  ptr + ii;
	*ptr = vv;
}

void bitmap_print(void* bm, int size) {
	return;
}

