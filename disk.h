#ifndef DISK_H
#define DISK_H

#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>

#include "bitmap.h"

// ==================== DISK INTERFACE ====================

typedef struct DiskInterface {
    int disk_file;                 // File handle for the disk image
    void* disk_base;
    uint64_t total_blocks;           // Total blocks available
    bool is_mounted;                 // Whether filesystem is mounted
} DiskInterface;

// Disk operations
DiskInterface* disk_open(const char* filename);
void disk_close(DiskInterface* disk);
void* get_block(DiskInterface* disk, int pnum);
void* get_superblock(DiskInterface* disk);
void* get_block_bitmap(DiskInterface* disk);
void* get_inode_bitmap(DiskInterface* disk);
void* get_inode_start(DiskInterface* disk);
int alloc_page(DiskInterface* disk);
void free_page(DiskInterface* disk, int pnum);
int disk_read_block(DiskInterface* disk, uint64_t block_num, void* buffer);
int disk_write_block(DiskInterface* disk, uint64_t block_num, const void* buffer);
int disk_format(DiskInterface* disk, const char* volume_name);

#endif

