#include "bitmap.h"

int bitmap_get(void* bm, int ii) {
	uint64_t* ptr = (uint64_t*)bm;
	ptr = ptr + ( ii / 64 );
	return (*ptr & (1 << (ii % 64))) >> (ii % 64);
}

void bitmap_put(void* bm, int ii, int vv) {
	uint64_t* ptr = (uint64_t*)bm;
	ptr = ptr + ( ii / 64 );
	*ptr = (vv==0) ? *ptr & ~(1 << (ii % 64)) : *ptr | (1 << (ii % 64));
	return;
}

void bitmap_print(void* bm, int size) {
	printf("===BITMAP START===\n");
	for (int ii = 0; ii < size; ++ii) {
            printf("%d", bitmap_get(bm, ii));
        }
        printf("\n===BITMAP END===\n");
	return;
}

