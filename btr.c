#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "btr.h"
#include "disk.h"
#include "hash.h"

// B-tree core operations
BTreeNode* btree_node_create(DiskInterface* disk, bool is_leaf)
{
	int page = alloc_page(disk);
	
	BTreeNode *node = (BTreeNode*)get_block(disk, page);
	
	node->block_number = page;
	
	node->is_leaf = is_leaf;
	node->key = 0;
	node->num_keys = 0;
	node->value = 0;
	node->parent = 0;
	
	for(int i=0; i<MAX_KEYS; i++) node->keys[i]=0;
	for(int i=0; i<=MAX_KEYS; i++) node->children[i]=0;
	
	return node;
}

void btree_node_free(DiskInterface* disk, BTreeNode* node)
{
	free_page(disk, node->block_number);
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

uint64_t btree_search(DiskInterface* disk, uint64_t node_block, uint64_t key)
{
	BTreeNode *node = (BTreeNode*)get_block(disk, node_block);
	
	if (node->is_leaf) {
		if (node->key == key) {
			printf("Found key!\n");
			return node->block_number;
		} else {
			printf("Did not find key!\n");
			return -1;
		}
	} else {
		for (int i = 0; i <= node->num_keys; i++) {
			if (node->children[i] != 0) {
				uint64_t result = btree_search(disk, node->children[i], key);
				if (result != -1) {
					return result;
				}
			}
		}
		printf("Did not find key!\n");
		return -1;
	}
}

int btree_find_depth(DiskInterface* disk, uint64_t node_block)
{
	BTreeNode *node = (BTreeNode*)get_block(disk, node_block);
	
	int depth=0;
	while (node->parent!=0)
	{
		node = (BTreeNode*)get_block(disk, node->parent);
		depth++;
	}
	return depth;
}

int btree_find_height(DiskInterface* disk, uint64_t node_block)
{
	BTreeNode *node = (BTreeNode*)get_block(disk, node_block);
	
	if (node->parent==0 && node->children[0]==0) return 0;
	
	int height=0;
	while (!node->is_leaf)
	{
		node = (BTreeNode*)get_block(disk, node->children[0]);
		height++;
	}
	return height;
}

int btree_find_minimum(DiskInterface* disk, uint64_t root_block)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	
	BTreeNode *first_child = (BTreeNode*)get_block(disk, root->children[0]);
	if (first_child->is_leaf) return first_child->key;
	else return btree_find_minimum(disk, first_child->block_number);
}

int btree_find_maximum(DiskInterface* disk, uint64_t root_block)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	
	if (root->is_leaf) return root->key;
	
	for (int i = root->num_keys; i >= 0; i--) {
		if (root->children[i] != 0) {
			return btree_find_maximum(disk, root->children[i]);
		}
	}
	
	return 0;
}

int btree_insert_nonfull(DiskInterface* disk, BTreeNode *root, BTreeNode *node)
{
	if (root->is_leaf) {
		printf("ERROR: Trying to insert into leaf node\n");
		return -1;
	} else {
		int i;
		for(i=0; i<root->num_keys && root->keys[i] < node->key; i++);
		
		for(int j=root->num_keys; j>i; j--) {
			root->keys[j] = root->keys[j-1];
			root->children[j+1] = root->children[j];
		}
		
		root->keys[i] = node->key;
		root->children[i+1] = node->block_number;
		node->parent = root->block_number;
		root->num_keys++;
		
		printf("Placing node with key %lu at position %d\n", node->key, i);
		printf("Block number = %lu\n", node->block_number);
	}
	
	return 0;
}

int btree_insertion_search(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *node = (BTreeNode*)get_block(disk, root_block);
	
	while (!node->is_leaf) {
		int i;
		for(i = 0; i < node->num_keys && key > node->keys[i]; i++);
		
		if (node->children[i] != 0) {
			node = (BTreeNode*)get_block(disk, node->children[i]);
		} else {
			break;
		}
	}
	
	return node->block_number;
}

void btree_update_parent_keys(DiskInterface* disk, BTreeNode* node)
{
	BTreeNode *current = node;
	
	while (current->parent != 0) {
		BTreeNode *parent = (BTreeNode*)get_block(disk, current->parent);
		
		int child_index = -1;
		for (int i = 0; i <= parent->num_keys; i++) {
			if (parent->children[i] == current->block_number) {
				child_index = i;
				break;
			}
		}
		
		if (child_index == -1) break;
		
		if (child_index > 0) {
			uint64_t max_key = btree_find_maximum(disk, current->block_number);
			parent->keys[child_index - 1] = max_key;
		}
		
		current = parent;
	}
}

int btree_insert(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	BTreeNode *node = btree_node_create(disk, true);
	node->key = key;
	
	if (root->is_leaf && root->num_keys == 0) {
		root->keys[0] = key;
		root->children[0] = node->block_number;
		node->parent = root->block_number;
		root->num_keys++;
		printf("Placing node with key %lu at position 0\n", key);
		printf("Block number = %lu\n", node->block_number);
		return 0;
	}
	
	if (root->num_keys == MAX_KEYS) {
		btree_split_root(disk, root);
	}
	
	btree_insert_nonfull(disk, root, node);
	
	return 0;
}

int btree_borrow_left(DiskInterface* disk, uint64_t root_block)
{
}

int btree_borrow_right(DiskInterface* disk, uint64_t root_block)
{
}

void btree_remove_key(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i] < key && root->keys[i]!=0; i++);
	printf("i=%d\n", i);
	// TODO: Merge children if num_keys < MIN_KEYS
	/*if (root->keys[i] == key)
	{
		printf("Removing key %ld from block %ld\n", key, root_block);
		for(int j=i; j<root->num_keys-1; j++)
		{
			root->keys[j] = root->keys[j+1];
			root->children[j+1] = root->children[j+2];
		}
		root->keys[root->num_keys-1] = 0;
		root->children[root->num_keys] = 0;
		root->num_keys--;
		if (root->num_keys==0)
		{
			root->keys[0] = btree_find_minimum(disk, root_block);
			root->num_keys++;
		}
		if (root->parent != 0)
		{
			btree_remove_key(disk, root->parent, key);
		}
	}*/
	
	return;
}

int btree_delete(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	int rv = btree_search(disk, root_block, key);
	BTreeNode *node;
	
	if (rv!=-1)
	{
		node = (BTreeNode*)get_block(disk, rv);
		btree_node_free(disk, node);
		btree_remove_key(disk, node->parent, key);
	}
	
	return rv;
}

void btree_split_root(DiskInterface* disk, BTreeNode* root)
{
	BTreeNode *child_a = btree_node_create(disk, false);
	BTreeNode *child_b = btree_node_create(disk, false);
	
	uint64_t promoted_key = root->keys[MIN_KEYS];
	
	for (int i = 0; i < MIN_KEYS; i++) {
		child_a->keys[i] = root->keys[i];
		child_a->children[i] = root->children[i];
		if (root->children[i] != 0) {
			BTreeNode *child = (BTreeNode*)get_block(disk, root->children[i]);
			child->parent = child_a->block_number;
		}
		child_a->num_keys++;
	}
	child_a->children[MIN_KEYS] = root->children[MIN_KEYS];
	if (root->children[MIN_KEYS] != 0) {
		BTreeNode *child = (BTreeNode*)get_block(disk, root->children[MIN_KEYS]);
		child->parent = child_a->block_number;
	}
	
	for (int i = MIN_KEYS + 1; i < root->num_keys; i++) {
		child_b->keys[i - MIN_KEYS - 1] = root->keys[i];
		child_b->children[i - MIN_KEYS - 1] = root->children[i];
		if (root->children[i] != 0) {
			BTreeNode *child = (BTreeNode*)get_block(disk, root->children[i]);
			child->parent = child_b->block_number;
		}
		child_b->num_keys++;
	}
	child_b->children[child_b->num_keys] = root->children[root->num_keys];
	if (root->children[root->num_keys] != 0) {
		BTreeNode *child = (BTreeNode*)get_block(disk, root->children[root->num_keys]);
		child->parent = child_b->block_number;
	}
	
	root->is_leaf = false;
	root->num_keys = 1;
	root->keys[0] = promoted_key;
	root->children[0] = child_a->block_number;
	root->children[1] = child_b->block_number;
	
	// Clear remaining slots
	for (int i = 1; i < MAX_KEYS; i++) {
		root->keys[i] = 0;
	}
	for (int i = 2; i <= MAX_KEYS; i++) {
		root->children[i] = 0;
	}
	
	child_a->parent = root->block_number;
	child_b->parent = root->block_number;
}

void btree_split_node(DiskInterface* disk, BTreeNode* node, int index, BTreeNode* child)
{
	BTreeNode *child_b = btree_node_create(disk, child->is_leaf);
	child_b->parent = node->block_number;
	
	uint64_t promoted_key = child->keys[MIN_KEYS];
	
	for (int i = MIN_KEYS + 1; i < child->num_keys; i++) {
		child_b->keys[i - MIN_KEYS - 1] = child->keys[i];
		child_b->num_keys++;
	}
	
	for (int i = MIN_KEYS + 1; i < child->num_keys; i++) {
		child->keys[i] = 0;
	}
	
	for (int i = MIN_KEYS + 1; i <= child->num_keys; i++) {
		child_b->children[i - MIN_KEYS - 1] = child->children[i];
		if (child_b->children[i - MIN_KEYS - 1] != 0) {
			BTreeNode *grandchild = (BTreeNode*)get_block(disk, child_b->children[i - MIN_KEYS - 1]);
			grandchild->parent = child_b->block_number;
		}
	}
	
	for (int i = MIN_KEYS + 1; i <= child->num_keys; i++) {
		child->children[i] = 0;
	}
	
	child->keys[MIN_KEYS] = 0;
	child->num_keys = MIN_KEYS;
	
	if (node->num_keys < MAX_KEYS) {
		for (int i = node->num_keys; i > index; i--) {
			node->keys[i] = node->keys[i - 1];
			node->children[i + 1] = node->children[i];
		}
		
		uint64_t max_key_of_left_child = btree_find_maximum(disk, child->block_number);
		node->keys[index] = max_key_of_left_child;
		node->children[index + 1] = child_b->block_number;
		node->num_keys++;
		
		btree_update_parent_keys(disk, child_b);
	} else {
		if (node->parent != 0) {
			BTreeNode *grandparent = (BTreeNode*)get_block(disk, node->parent);
			int parent_index;
			for (parent_index = 0; parent_index <= grandparent->num_keys; parent_index++) {
				if (grandparent->children[parent_index] == node->block_number) break;
			}
			btree_split_node(disk, grandparent, parent_index, node);
			
			BTreeNode *current_parent = (BTreeNode*)get_block(disk, child->parent);
			int new_index;
			for (new_index = 0; new_index <= current_parent->num_keys; new_index++) {
				if (current_parent->children[new_index] == child->block_number) break;
			}
			
			if (current_parent->num_keys < MAX_KEYS) {
				for (int i = current_parent->num_keys; i > new_index; i--) {
					current_parent->keys[i] = current_parent->keys[i - 1];
					current_parent->children[i + 1] = current_parent->children[i];
				}
				uint64_t max_key_of_left_child = btree_find_maximum(disk, child->block_number);
				current_parent->keys[new_index] = max_key_of_left_child;
				current_parent->children[new_index + 1] = child_b->block_number;
				current_parent->num_keys++;
				
				// Update parent keys iteratively
				btree_update_parent_keys(disk, child_b);
			}
		} else {
			btree_split_root(disk, node);
			
			BTreeNode *new_root = node;
			for (int i = new_root->num_keys; i > index; i--) {
				new_root->keys[i] = new_root->keys[i - 1];
				new_root->children[i + 1] = new_root->children[i];
			}
			uint64_t max_key_of_left_child = btree_find_maximum(disk, child->block_number);
			new_root->keys[index] = max_key_of_left_child;
			new_root->children[index + 1] = child_b->block_number;
			new_root->num_keys++;
			
			btree_update_parent_keys(disk, child_b);
		}
	}
}

void btree_merge_children(DiskInterface* disk, BTreeNode* parent, int index)
{
	BTreeNode *child_a = (BTreeNode*)get_block(disk, parent->children[index]);
	BTreeNode *child_b = (BTreeNode*)get_block(disk, parent->children[index+1]);
	
	for (int i = MIN_KEYS + 1; i < MAX_KEYS; i++) {
		child_a->keys[i] = child_b->keys[i - MIN_KEYS - 1];
		child_a->num_keys++;
	}
	for (int i = MIN_KEYS + 1; i <= MAX_KEYS; i++) {
		child_a->keys[i] = child_b->children[i - MIN_KEYS - 1];
	}
	
	if (index == MAX_KEYS-1) parent->keys[index] = 0;
	for(int i=index+1; j<MAX_KEYS-1; j++)
	{
		root->keys[i] = root->keys[i+1];
	}
	for(int i=index+1; j<MAX_KEYS; j++)
	{
		root->children[i] = root->children[i+1];
	}
	
	btree_update_parent_keys(disk, child_a);
	btree_node_free(disk, child_b);
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

void debug_print_node(DiskInterface* disk, uint64_t block_num, int level) {
	BTreeNode *node = (BTreeNode*)get_block(disk, block_num);
	printf("%*sBlock %lu: ", level*2, "", block_num);
	
	if (node->is_leaf) {
		printf("LEAF key=%lu parent=%lu\n", node->key, node->parent);
	} else {
		printf("INTERNAL keys=[");
		for(int i = 0; i < node->num_keys; i++) {
			printf("%lu", node->keys[i]);
			if (i < node->num_keys-1) printf(",");
		}
		printf("] children=[");
		for(int i = 0; i <= node->num_keys; i++) {
			printf("%lu", node->children[i]);
			if (i < node->num_keys) printf(",");
		}
		printf("]\n");
		
		// Recursively print children
		for(int i = 0; i <= node->num_keys; i++) {
			if (node->children[i] != 0) {
				debug_print_node(disk, node->children[i], level+1);
			}
		}
	}
}

int main()
{
	DiskInterface* disk = disk_open("my.img");
	alloc_page(disk);
	BTreeNode *root = btree_node_create(disk, false);
	
	while (true) {
		printf("Select 1 to insert a key, and 2 to search for a key, and 3 for debug print: ");
		int choice, key;
		scanf("%d", &choice);
		switch (choice) {
			case 1:
				printf("Key to insert: ");
				scanf("%d", &key);
				btree_insert(disk, root->block_number, key);
				break;
			case 2:
				printf("Key to search: ");
				scanf("%d", &key);
				btree_search(disk, root->block_number, key);
				break;
			case 3:
				debug_print_node(disk, root->block_number, 1);
				break;
			default:
				return 0;
		}
	}
}
