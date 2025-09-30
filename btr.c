#include <stdio.h>
#include <limits.h>
#include <string.h>
#include "btr.h"
#include "disk.h"
#include "hash.h"

// B-tree core operations
BTreeNode* btree_node_create(DiskInterface* disk, bool is_leaf)
{
	BTreeNode stack_node;
	stack_node.is_leaf = is_leaf;
	stack_node.key = 0;
	stack_node.num_keys = 0;
	memset(&stack_node.keys, 0, MAX_KEYS);
	memset(&stack_node.children, 0, MAX_KEYS+1);
	
	int page = alloc_page(disk);
	
	stack_node.block_number = page;
	
	BTreeNode *mmap_node = (BTreeNode*)get_block(disk, page);
	
	memcpy((char*)mmap_node, (char*)&stack_node, sizeof(BTreeNode));
	
	return mmap_node;
}

void btree_node_free(BTreeNode* node)
{
}

int btree_node_read(DiskInterface* disk, uint64_t block_num, BTreeNode* node)
{
	int rv;
	BTreeNode *disk_node = (BTreeNode*)get_block(disk, block_num);
	
	void *ptr = memcpy((char*)node, (char*)disk_node, sizeof(BTreeNode));
	
	rv = (ptr==NULL) ? -1 : 0;
	
	return rv;
}

int btree_node_write(DiskInterface* disk, BTreeNode* node)
{
	int rv;
	BTreeNode *mem_node = (BTreeNode*)get_block(disk, node->block_number);
	
	void *ptr = memcpy((char*)mem_node, (char*)node, sizeof(BTreeNode));
	
	rv = (ptr==NULL) ? -1 : 0;
	
	return rv;
}

uint64_t btree_search(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
}

int btree_insert(DiskInterface* disk, uint64_t root_block, uint64_t key, uint64_t value)
{
	// TODO: Implement me first!! Might have to change root_block pointer...
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	BTreeNode *node = btree_node_create(disk, true);
	node->key = key;
	node->block_number = value;
	
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i] < key && root->keys[i]!=0; i++);
	

}

int btree_delete(DiskInterface* disk, uint64_t* root_block, uint64_t key)
{
}

void btree_split_node(DiskInterface* disk, BTreeNode* node, int index)
{
	BTreeNode new_root;
	new_root.is_leaf = false;
	new_root.key = 0;
	new_root.num_keys = 1;
	new_root.keys[0] = node->keys[index];
	
	BTreeNode *child_a = btree_node_create(disk, false);
	BTreeNode *child_b = btree_node_create(disk, false);
	
	int i, j;
	for (i=0, j=0; i<index; i++, j++)
	{
		child_a->keys[j] = node->keys[i];
	}
	for (i=index+1, j=0; i<MAX_KEYS; i++, j++)
	{
		child_a->keys[j] = node->keys[i];
	}
	
	memcpy((char*)node, (char*)&new_root, sizeof(BTreeNode));
	
	return;
}

void btree_merge_children(DiskInterface* disk, BTreeNode* parent, int index)
{
}

// B-tree traversal and debugging
void btree_traverse(DiskInterface* disk, uint64_t root_block, void (*callback)(uint64_t key, uint64_t value))
{
}

void btree_validate(DiskInterface* disk, uint64_t root_block)
{
}

void btree_print(DiskInterface* disk, uint64_t root_block, int level)
{
}

int main()
{
	// TODO: Implement B-Tree operations and begin testing them!!
	printf("hash '/' : %u\n", hash("/"));
	printf("hash '/a' : %u\n", hash("/a"));
	printf("hash '/b' : %u\n", hash("/b"));
	printf("hash '/c' : %u\n", hash("/c"));
	printf("sizeof(node) : %lu\n", sizeof(BTreeNode));
}
