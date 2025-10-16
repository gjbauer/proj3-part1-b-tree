#include "hash.h"
#include <stdint.h>

// Simple FNV-1a implementation for filesystem
uint64_t path_hash(const char *path) {
    uint64_t hash = 0xcbf29ce484222325ULL; // FNV offset basis
    
    for (; *path; ++path) {
        hash ^= (uint64_t)(unsigned char)(*path);
        hash *= 0x100000001b3ULL; // FNV prime
    }
    
    return hash;
}

