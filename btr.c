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
	
	for(int i=0; i<MAX_KEYS; i++) stack_node.keys[i]=0;
	for(int i=0; i<=MAX_KEYS; i++) stack_node.children[i]=0;
	
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
	int rv = -1;
	printf("Searching for key %d\n", key);
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i]!=0; i++)
	{
		if (root->keys[i] >= key) break;
	}
	
	if (root->key == key) rv = root->block_number;
	else if (root->keys[i] == key) {
		btree_search(disk, root->children[i+1], key);
	}
	else if (root->keys[i] > key && root->children[i]!=0)
	{
		btree_search(disk, root->children[i], key);
	}
	else printf("Did not find key!\n");
	
}

int btree_insert_nonfull(BTreeNode *root, BTreeNode *node)
{
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i] < node->key && root->keys[i]!=0; i++);
	if (root->keys[i]!=0 && root->keys[i] > node->key)
	{
		for(int j=MAX_KEYS-1; j>i; j--)
		{
			root->keys[j] = root->keys[j-1];
			root->children[j] = root->children[j];
		}
		root->keys[i] = node->key;
		root->num_keys++;
	}
	else if (!root->keys[i]) root->keys[i] = node->key, root->num_keys++;;
	printf("Placing node with key %lu at position %d\n", node->key, i+1);
	printf("Block number = %d\n", node->block_number);
	root->children[i+1] = node->block_number;
}

int btree_insert(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	BTreeNode *node = btree_node_create(disk, true);
	node->key = key;
	
	if (root->num_keys < MAX_KEYS)
	{
		btree_insert_nonfull(root, node);
	}
	else
	{
		if (root->keys[0] > key)
		{
			printf("Placing node with key %lu at position %d\n", key, 0);
			printf("Block number = %d\n", node->block_number);
			root->children[0] = node->block_number;
		}
		else
		{
			int index;
			if (key < root->keys[MIN_KEYS]) index = MIN_KEYS-1;
			else index = MIN_KEYS;
			btree_split_node(disk, root, index);
			btree_insert_nonfull(root, node);
		}
	}
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
	new_root.block_number = node->block_number;
	
	BTreeNode *child_a = btree_node_create(disk, false);
	BTreeNode *child_b = btree_node_create(disk, false);
	
	new_root.children[0] = child_a->block_number;
	new_root.children[1] = child_b->block_number;
	
	int i, j;
	for (i=0, j=0; i<index; i++, j++, child_a->num_keys++)
	{
		child_a->keys[j] = node->keys[i];
	}
	for (i=0, j=0; i<=index; i++, j++)
	{
		child_a->children[j] = node->children[i];
	}
	for (i=index, j=0; i<MAX_KEYS; i++, j++, child_b->num_keys++)
	{
		child_b->keys[j] = node->keys[i];
	}
	for (i=index+1, j=1; i<=MAX_KEYS; i++, j++)
	{
		child_b->children[j] = node->children[i];
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
	DiskInterface* disk = disk_open("my.img");
	alloc_page(disk);
	void* pbm = get_block_bitmap(disk);
	bitmap_print(pbm, disk->total_blocks);
	BTreeNode *root = btree_node_create(disk, false);
	btree_insert(disk, root->block_number, hash("/a"));
	bitmap_print(pbm, disk->total_blocks);
	//btree_insert(disk, root->block_number, hash("/"));
	//btree_split_node(disk, root, 1);
	btree_insert(disk, root->block_number, hash("/b"));
	btree_insert(disk, root->block_number, hash("/"));
	btree_insert(disk, root->block_number, hash("/c"));
	btree_insert(disk, root->block_number, hash("/d"));
	btree_insert(disk, root->block_number, hash("/e"));
	bitmap_print(pbm, disk->total_blocks);
	btree_search(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/b"));
	btree_search(disk, root->block_number, hash("/f"));
}
