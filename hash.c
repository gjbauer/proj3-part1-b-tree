#include "hash.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned int hash(const unsigned char *str)
{
    unsigned long hash = 137;
    int c;

    while ( (c = *str++) )
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

/*int main() {
	printf("%ld\n", hash("hello") % 512);
}*/

