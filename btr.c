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
		}
		for(int j=root->num_keys+1; j>i+1; j--) {
			root->children[j] = root->children[j-1];
		}
		
		root->keys[i] = node->key;
		root->children[i] = node->block_number;
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
	
	if (node->children[0] == 0) {
		return node->block_number;
	}
	
	while (true) {
		bool has_leaf_child = false;
		for (int i = 0; i <= node->num_keys; i++) {
			if (node->children[i] != 0) {
				BTreeNode *child = (BTreeNode*)get_block(disk, node->children[i]);
				if (child->is_leaf) {
					has_leaf_child = true;
					break;
				}
			}
		}
		
		if (has_leaf_child) {
			return node->block_number;
		}
		
		int child_index = 0;
		for (int i = 0; i < node->num_keys; i++) {
			if (key <= node->keys[i]) {
				child_index = i;
				break;
			} else {
				child_index = i + 1;
			}
		}
		
		if (node->children[child_index] != 0) {
			node = (BTreeNode*)get_block(disk, node->children[child_index]);
		} else {
			return node->block_number;
		}
	}
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
		
		// Update the key that separates this child from the next
		if (child_index > 0) {
			uint64_t max_key = btree_find_maximum(disk, current->block_number);
			// Only update if the key has actually changed
			if (parent->keys[child_index - 1] != max_key) {
				parent->keys[child_index - 1] = max_key;
			}
		}
		
		current = parent;
	}
}

int btree_insert(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	BTreeNode *node = btree_node_create(disk, true);
	node->key = key;
	
	int target_block = btree_insertion_search(disk, root_block, key);
	BTreeNode *target = (BTreeNode*)get_block(disk, target_block);
	
	if (target->num_keys == MAX_KEYS) {
		if (target->keys[MAX_KEYS - 1] < key && target->children[MAX_KEYS]==0)
		{
			target->children[MAX_KEYS]=node->block_number;
			node->parent=target->block_number;
		} else {
			if (target->parent != 0) {
				BTreeNode *parent = (BTreeNode*)get_block(disk, target->parent);
				int i;
				for(i=0; i<MAX_KEYS && parent->keys[i] < btree_find_maximum(disk, target->block_number) && parent->keys[i]!=0; i++);
				btree_split_node(disk, parent, i, target);
				target_block = btree_insertion_search(disk, root_block, key);
				target = (BTreeNode*)get_block(disk, target_block);
			} else {
				btree_split_root(disk, target);
				target_block = btree_insertion_search(disk, root_block, key);
				target = (BTreeNode*)get_block(disk, target_block);
			}
			btree_insert_nonfull(disk, target, node);
		}
	}
	else btree_insert_nonfull(disk, target, node);
	
	return 0;
}

int btree_borrow_left(DiskInterface* disk, uint64_t root_block, int index)
{
	// TODO: Implement borrowing from left sibling
	return -1;
}

int btree_borrow_right(DiskInterface* disk, uint64_t root_block, int index)
{
	// TODO: Implement borrowing from right sibling
	return -1;
}

void btree_remove_key(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i] < key && root->keys[i]!=0; i++);
	printf("i=%d\n", i);
	printf("root->keys[i]=%lu\n", root->keys[i]);
	printf("key=%lu\n", key);
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
		child_b->num_keys++;
	}
	for (int i = MIN_KEYS + 1; i <= root->num_keys; i++) {
		child_b->children[i - MIN_KEYS - 1] = root->children[i];
		if (root->children[i] != 0) {
			BTreeNode *child = (BTreeNode*)get_block(disk, root->children[i]);
			child->parent = child_b->block_number;
		}
	}
	printf("btree_find_maximum = %d\n", btree_find_maximum(disk, child_b->block_number));
	btree_print(disk, root->block_number, 1);
	if (root->children[MAX_KEYS]!=0) child_b->keys[child_b->num_keys++]=btree_find_maximum(disk, child_b->block_number);
	if (root->children[root->num_keys] != 0) {
		BTreeNode *child = (BTreeNode*)get_block(disk, root->children[root->num_keys]);
		child->parent = child_b->block_number;
	}
	
	root->is_leaf = false;
	root->num_keys = 1;
	root->keys[0] = btree_find_maximum(disk, child_a->block_number);
	root->children[0] = child_a->block_number;
	root->children[1] = child_b->block_number;
	
	btree_print(disk, root->block_number, 1);
	
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
		child->children[i] = 0;
	}
	
	child->keys[MIN_KEYS] = 0;
	child->num_keys = MIN_KEYS;
	
	if (node->num_keys < MAX_KEYS) {
		for (int i = node->num_keys; i > index; i--) {
			node->keys[i] = node->keys[i - 1];
		}
		for (int i = node->num_keys+1; i > index; i--) {
			node->children[i] = node->children[i-1];
		}
		
		node->keys[index] = btree_find_maximum(disk, child->block_number);
		node->children[index + 1] = child_b->block_number;
		node->num_keys++;
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
				}
				for (int i = current_parent->num_keys+1; i > new_index; i--) {
					current_parent->children[i] = current_parent->children[i-1];
				}
				
				current_parent->keys[new_index] = btree_find_maximum(disk, child->block_number);
				current_parent->children[new_index + 1] = child_b->block_number;
				current_parent->num_keys++;
			}
		} else {
			btree_split_root(disk, node);
			
			BTreeNode *new_root = node;
			for (int i = new_root->num_keys; i > index; i--) {
				new_root->keys[i] = new_root->keys[i - 1];
			}
			for (int i = new_root->num_keys + 1; i > index; i--) {
				new_root->children[i] = new_root->children[i-1];
			}
			
			new_root->keys[index] = btree_find_maximum(disk, child->block_number);
			new_root->children[index + 1] = child_b->block_number;
			new_root->num_keys++;
		}
	}
}

void btree_merge_children(DiskInterface* disk, BTreeNode* parent, int index)
{
	BTreeNode *child_a = (BTreeNode*)get_block(disk, parent->children[index]);
	BTreeNode *child_b = (BTreeNode*)get_block(disk, parent->children[index+1]);
	
	// TODO: Should we delete keys first and then worry about borrow/merge, or should we borrow/merge first, then worry about deletion?
	for (int i = MIN_KEYS + 1; i < MAX_KEYS; i++) {
		child_a->keys[i] = child_b->keys[i - MIN_KEYS - 1];
		child_a->num_keys++;
	}
	for (int i = MIN_KEYS + 1; i <= MAX_KEYS; i++) {
		child_a->keys[i] = child_b->children[i - MIN_KEYS - 1];
	}
	
	for(int i=index+1; i<MAX_KEYS-1; i++)
	{
		parent->keys[i] = parent->keys[i+1];
	}
	parent->keys[MAX_KEYS - 1] = 0;
	
	for(int i=index+1; i<MAX_KEYS; i++)
	{
		parent->children[i] = parent->children[i+1];
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
	BTreeNode *node = (BTreeNode*)get_block(disk, root_block);
	printf("%*sBlock %lu: ", level*2, "", root_block);
	
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
				btree_print(disk, node->children[i], level+1);
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
				btree_print(disk, root->block_number, 1);
				break;
			default:
				return 0;
		}
	}
}
