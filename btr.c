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
	node->left_sibling = 0;
	node->right_sibling = 0;
	
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
	while (true) {
		if (node->is_leaf) {
			return depth;
		}
	
		if (node->children[0] != 0) {
			node = (BTreeNode*)get_block(disk, node->children[0]);
			depth++;
		} else {
			printf("ERROR: Node is not leaf and child not found!!\n");
		}
	}
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
		int child_pos = 0;
		for(int j = 0; j <= root->num_keys; j++) {
			if(root->children[j] != 0) {
				BTreeNode *child = (BTreeNode*)get_block(disk, root->children[j]);
				if(child->is_leaf && child->key < node->key) {
					child_pos = j + 1;
				} else if(!child->is_leaf) {
					uint64_t max_key = btree_find_maximum(disk, child->block_number);
					if(max_key < node->key) {
						child_pos = j + 1;
					}
				}
			}
		}
		
		for(int j = root->num_keys; j >= child_pos; j--) {
			root->children[j + 1] = root->children[j];
		}
		
		root->children[child_pos] = node->block_number;
		node->parent = root->block_number;
		root->num_keys++;
		
		for(int i = 0; i < root->num_keys; i++) {
			if(root->children[i] != 0) {
				root->keys[i] = btree_find_maximum(disk, root->children[i]);
			}
		}
		
		printf("Placing node with key %lu at child position %d\n", node->key, child_pos);
		printf("Block number = %lu\n", node->block_number);
	}
	
	return 0;
}

int btree_insertion_search(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	
	if (root->children[0] == 0) {
		return root->block_number;
	}
	
	if (btree_find_depth(disk, root_block)-1 <= 0) {
		return root->block_number;
	}
	
	BTreeNode *node = root;
	int current_depth = 0;
	while (current_depth < btree_find_depth(disk, root_block)-1) {
		int child_index = 0;
		
		for (int i = 0; i < node->num_keys; i++) {
			if (node->children[i] != 0) {
				uint64_t child_max = btree_find_maximum(disk, node->children[i]);
				if (key <= child_max) {
					child_index = i;
					break;
				}
				child_index = i + 1;
			}
		}
		
		if (child_index > node->num_keys) {
			child_index = node->num_keys;
		}
		
		bool descended = false;
		if (node->children[child_index] != 0) {
			node = (BTreeNode*)get_block(disk, node->children[child_index]);
			current_depth++;
			descended = true;
		} else {
			for (int i = node->num_keys; i >= 0; i--) {
				if (node->children[i] != 0) {
					node = (BTreeNode*)get_block(disk, node->children[i]);
					current_depth++;
					descended = true;
					break;
				}
			}
		}
		
		if (!descended) {
			break;
		}
	}
	
	return node->block_number;
}

void btree_update_parent_keys(DiskInterface* disk, BTreeNode* node)
{
	if (node->parent == 0) return;
	
	BTreeNode *parent = (BTreeNode*)get_block(disk, node->parent);
	
	// Update all keys in the parent based on the maximum values of each child
	for (int i = 0; i < parent->num_keys; i++) {
		if (parent->children[i] != 0) {
			parent->keys[i] = btree_find_maximum(disk, parent->children[i]);
		}
	}
}

int btree_insert(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
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
				btree_split_child(disk, parent, i, target);
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

	btree_update_parent_keys(disk, node);
	
	return 0;
}

int btree_borrow_left(DiskInterface* disk, BTreeNode *node)
{
	int rv = -1;
	if (node->left_sibling==0) return rv;
	else {
		BTreeNode *left_sibling = (BTreeNode*)get_block(disk, node->left_sibling);
		
		if (left_sibling->num_keys==MIN_KEYS) return rv;
		else {
			for (int i = left_sibling->num_keys; i >= 0; i--) {
				if (left_sibling->children[i] != 0) {
					if (i < left_sibling->num_keys) left_sibling->keys[i]=0;
					rv = left_sibling->children[i];
					left_sibling->children[i] = 0;
					left_sibling->num_keys--;
					break;
				}
			}
		}
	}
	return rv;
}

int btree_borrow_right(DiskInterface* disk, BTreeNode *node)
{
	int rv = -1;
	if (node->right_sibling==0) return rv;
	else {
		BTreeNode *right_sibling = (BTreeNode*)get_block(disk, node->right_sibling);
		
		if (right_sibling->num_keys==MIN_KEYS) return rv;
		else {
			rv = right_sibling->children[0];
			for(int i = 0; i < MAX_KEYS; i++) {
				right_sibling->keys[i] = right_sibling->keys[i+1];
			}
			for(int i = 0; i <= MAX_KEYS; i++) {
				right_sibling->children[i] = right_sibling->children[i+1];
			}
			right_sibling->keys[MAX_KEYS-1] = 0;
			right_sibling->children[MAX_KEYS] = 0;
			right_sibling->num_keys--;
		}
	}
	return rv;
}

void btree_remove_key(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i] < key && root->keys[i]!=0; i++);
	BTreeNode *node = (BTreeNode*)get_block(disk, root->children[i]);
	BTreeNode *borrowed;
	int rv;
	if (root->num_keys==MIN_KEYS)
	{
		rv = btree_borrow_left(disk, root);
		if (rv==-1) {
			rv = btree_borrow_right(disk, root);
			if (rv==-1) {
				BTreeNode *grandparent = (BTreeNode*)get_block(disk, root->parent);
				int j;
				for(j=0; j<MAX_KEYS && grandparent->keys[j] < key && grandparent->keys[j]!=0; j++);
				btree_merge_children(disk, grandparent, j);
			}
		}
	}
	for(i=0; i<MAX_KEYS && root->keys[i] < key && root->keys[i]!=0; i++);
	printf("Removing key %ld from block %ld\n", key, root_block);
	for(int j=i; j<root->num_keys; j++)
	{
		root->keys[j] = root->keys[j+1];
	}
	for(int j=i; j<=root->num_keys; j++)
	{
		root->children[j] = root->children[j+1];
	}
	root->keys[root->num_keys-1] = 0;
	root->children[root->num_keys] = 0;
	root->num_keys--;
	if (root->num_keys==0)
	{
		root->keys[0] = btree_find_maximum(disk, root_block);
		root->num_keys++;
	}
	
	if (rv!=-1) {
		borrowed = (BTreeNode*)get_block(disk, rv);
		btree_insert_nonfull(disk, root, borrowed);
	}
	btree_update_parent_keys(disk, node);
	
	return;
}

int btree_delete(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	int rv = btree_search(disk, root_block, key);
	BTreeNode *node;
	
	if (rv!=-1)
	{
		node = (BTreeNode*)get_block(disk, rv);
		btree_remove_key(disk, node->parent, key);
		btree_node_free(disk, node);
	}
	
	return rv;
}

void btree_split_root(DiskInterface* disk, BTreeNode* root)
{
	BTreeNode *child_a = btree_node_create(disk, false);
	BTreeNode *child_b = btree_node_create(disk, false);
	child_a->right_sibling = child_b->block_number;
	child_b->left_sibling = child_a->block_number;
	
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
	
	if (root->children[MAX_KEYS] != 0) {
		child_b->children[child_b->num_keys] = root->children[MAX_KEYS];
		BTreeNode *child = (BTreeNode*)get_block(disk, root->children[MAX_KEYS]);
		child->parent = child_b->block_number;
	}
	
	root->is_leaf = false;
	root->num_keys = 1;
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
	root->keys[0] = btree_find_maximum(disk, child_a->block_number);
}

void btree_promote_root(DiskInterface* disk, BTreeNode* root)
{
	int page = root->block_number;
	
	BTreeNode *first_child = (BTreeNode*)get_block(disk, root->children[0]);
	
	memcpy(root, first_child, sizeof(BTreeNode));
	
	root->block_number = page;
	root->parent = 0;
	
	btree_node_free(disk, first_child);
	
	BTreeNode *child;
	for (int i = 0; i <= root->num_keys; i++) {
		if (root->children[i] != 0) {
			child = (BTreeNode*)get_block(disk, root->children[i]);
			child->parent = root->block_number;
		}
	}
}

void btree_split_child(DiskInterface* disk, BTreeNode* node, int index, BTreeNode* child)
{
	BTreeNode *child_b = btree_node_create(disk, false);
	child_b->parent = node->block_number;
	child->right_sibling = child_b->block_number;
	child_b->left_sibling = child->block_number;
	
	for (int i = MIN_KEYS + 1; i < child->num_keys; i++) {
		child_b->keys[i - MIN_KEYS - 1] = child->keys[i];
		child->keys[i] = 0;
		child_b->num_keys++;
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
			node->children[i + 1] = node->children[i];
		}
		
		node->children[index + 1] = child_b->block_number;
		node->keys[index] = btree_find_maximum(disk, child_b->block_number);
		child_b->parent = node->block_number;
		node->num_keys++;
		for (int i = 0; i < node->num_keys; i++) {
			if (node->children[i] != 0) {
				node->keys[i] = btree_find_maximum(disk, node->children[i]);
			}
		}
	} else {
		if (node->parent != 0) {
			BTreeNode *grandparent = (BTreeNode*)get_block(disk, node->parent);
			int parent_index;
			for (parent_index = 0; parent_index <= grandparent->num_keys; parent_index++) {
				if (grandparent->children[parent_index] == node->block_number) break;
			}
			btree_split_child(disk, grandparent, parent_index, node);
		} else {
			btree_split_root(disk, node);
		}
		
		BTreeNode *current_parent = (BTreeNode*)get_block(disk, child->parent);
		int new_index;
		for (new_index = 0; new_index <= current_parent->num_keys; new_index++) {
			if (current_parent->children[new_index] == child->block_number) break;
		}
		
		if (current_parent->num_keys < MAX_KEYS) {
			for (int i = current_parent->num_keys; i > new_index; i--) {
				current_parent->children[i + 1] = current_parent->children[i];
			}
			
			current_parent->children[new_index + 1] = child_b->block_number;
			
			current_parent->num_keys++;
			for (int i = 0; i < current_parent->num_keys; i++) {
				if (current_parent->children[i] != 0) {
					current_parent->keys[i] = btree_find_maximum(disk, current_parent->children[i]);
				}
			}
		}
	}
}

void btree_merge_children(DiskInterface* disk, BTreeNode* parent, int index)
{
	if (index == MAX_KEYS) return btree_merge_children(disk, parent, index-1);
	else if (parent->children[index] == 0 || parent->children[index+1]==0) return;
	BTreeNode *child_a = (BTreeNode*)get_block(disk, parent->children[index]);
	BTreeNode *child_b = (BTreeNode*)get_block(disk, parent->children[index+1]);
	child_a->right_sibling = child_b->right_sibling;
	
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
	
	btree_node_free(disk, child_b);
	
	parent->num_keys--;
	
	if (parent->num_keys < MIN_KEYS) {
		if (parent->parent == 0) {
			// TODO: Promote root
			if (parent->children[1]==0) {
				printf("Promoting root!\n");
				btree_promote_root(disk, root);
			}
		} else {
			int rv = btree_borrow_left(disk, root);
			if (rv==-1) {
				rv = btree_borrow_right(disk, root);
				if (rv==-1) {
					BTreeNode *grandparent = (BTreeNode*)get_block(disk, root->parent);
					int j;
					for(j=0; j<MAX_KEYS && grandparent->keys[j] < btree_find_maximum(disk, parent) && grandparent->keys[j]!=0; j++);
					btree_merge_children(disk, grandparent, j);
				}
			}
		}
	}
}

// B-tree debugging
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
		printf("Select 1 to insert a key, and 2 to search for a key, and 3 for debug print, and 4 to delete a key: ");
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
			case 4:
				printf("Key to delete: ");
				scanf("%d", &key);
				btree_delete(disk, root->block_number, key);
				break;
			default:
				return 0;
		}
	}
}
