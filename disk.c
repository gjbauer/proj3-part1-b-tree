#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <string.h>

#include "disk.h"
#include "config.h"

// Disk operations
DiskInterface* disk_open(const char* filename)
{
	DiskInterface *disk = (DiskInterface*)malloc(sizeof(DiskInterface));
	struct stat fs_info;
	
	if (stat(filename, &fs_info) != 0) {
		fprintf(stderr, "Failed to stat filesystem!!");
		return NULL;
	}
	
	disk->disk_file = open(filename, O_RDWR, 0644);
	assert(disk->disk_file != -1);
	
	disk->disk_base = mmap(0, fs_info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk->disk_file, 0);
	assert(disk->disk_base != MAP_FAILED);
	
	disk->total_blocks = fs_info.st_size / BLOCK_SIZE;
	
	return disk;
}

void disk_close(DiskInterface* disk)
{
	munmap(disk->disk_base, disk->total_blocks * BLOCK_SIZE);
	close(disk->disk_file);
	free(disk);
}

void*
get_block(DiskInterface* disk, int pnum)
{
    return disk->disk_base + BLOCK_SIZE * pnum;
}

void*
get_superblock(DiskInterface* disk)
{
    return get_block(disk, 0);
}


void*
get_block_bitmap(DiskInterface* disk)
{
    return get_block(disk, 1);
}

void*
get_inode_bitmap(DiskInterface* disk)
{
    return get_block(disk, 2);
}

void*
get_inode_start(DiskInterface* disk)
{
    return get_block(disk, 3);
}

/* void*
 * get_root_start()
 * {
 *     return get_block(disk, 6);	TODO: Have a minimum partition size and set the correct corresponding block offset for the root inode
 * }
 */

int
alloc_page(DiskInterface* disk)
{
    void* pbm = get_block_bitmap(disk);

    for (int ii = 1; ii < disk->total_blocks; ++ii) {
        if (!bitmap_get(pbm, ii)) {
            bitmap_put(pbm, ii, 1);
            printf("+ alloc_page() -> %d\n", ii);
            return ii;
        }
    }

    return -1;
}

void
free_page(DiskInterface* disk, int pnum)
{
    printf("+ free_page(%d)\n", pnum);
    void* pbm = get_block_bitmap(disk);
    bitmap_put(pbm, pnum, 0);
}

int disk_read_block(DiskInterface* disk, uint64_t block_num, void* buffer)
{
	int rv = -1;
	void *block = get_block(disk, block_num);
	
	if (memcpy(buffer, block, BLOCK_SIZE)) {
		rv = 0;
	}
	
	return rv;
}

int disk_write_block(DiskInterface* disk, uint64_t block_num, const void* buffer)
{
	int rv = -1;
	void *block = get_block(disk, block_num);
	
	if (memcpy(block, buffer, BLOCK_SIZE)) {
		rv = 0;
	}
	
	return rv;
}

int disk_format(DiskInterface* disk, const char* volume_name);
