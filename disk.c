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
	
	if (stat(filename, &disk->fs_info) != 0) {
		fprintf(stderr, "Failed to stat filesystem!!");
		return NULL;
	}
	
	disk->disk_file = open(filename, O_RDWR, 0644);
	assert(disk->disk_file != -1);
	
	disk->disk_base = mmap(0, disk->fs_info.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disk->disk_file, 0);
	assert(disk->disk_base != MAP_FAILED);
	
	disk->total_blocks = disk->fs_info.st_size / BLOCK_SIZE;
	
	return disk;
}

void disk_close(DiskInterface* disk)
{
	munmap(disk->disk_base, disk->fs_info.st_size);
	close(disk->disk_file);
	free(disk);
}

void*
pages_get_page(DiskInterface* disk, int pnum)
{
    return disk->disk_base + BLOCK_SIZE * pnum;
}

void*
get_superblock()
{
    return pages_get_page(0);
}


void*
get_pages_bitmap()
{
    return pages_get_page(1);
}

void*
get_inode_bitmap()
{
    return pages_get_page(2);
}

void*
get_inode_start()
{
    return pages_get_page(3);
}

/* void*
 * get_root_start()
 * {
 *     return pages_get_page(6);	TODO: Have a minimum partition size and set the correct corresponding block offset for the root inode
 * }
 */

int
alloc_page()
{
    void* pbm = get_pages_bitmap();

    for (int ii = 1; ii < PAGE_COUNT; ++ii) {
        if (!bitmap_get(pbm, ii)) {
            bitmap_put(pbm, ii, 1);
            printf("+ alloc_page() -> %d\n", ii);
            return ii;
        }
    }

    return -1;
}

void
free_page(int pnum)
{
    printf("+ free_page(%d)\n", pnum);
    void* pbm = get_pages_bitmap();
    bitmap_put(pbm, pnum, 0);
}

int disk_read_block(DiskInterface* disk, uint64_t block_num, void* buffer)
{
	int rv = -1;
	void *block = pages_get_page(block_num);
	
	if (memcpy(buffer, block, BLOCK_SIZE)) {
		rv = 0;
	}
	
	return rv;
}

int disk_write_block(DiskInterface* disk, uint64_t block_num, const void* buffer)
{
	int rv = -1;
	void *block = pages_get_page(block_num);
	
	if (memcpy(block, buffer, BLOCK_SIZE)) {
		rv = 0;
	}
	
	return rv;
}

int disk_format(DiskInterface* disk, const char* volume_name);
