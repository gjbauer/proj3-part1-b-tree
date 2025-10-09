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
	BTreeNode stack_node;
	stack_node.is_leaf = is_leaf;
	stack_node.key = 0;
	stack_node.num_keys = 0;
	stack_node.value = 0;
	stack_node.parent = 0;
	
	for(int i=0; i<MAX_KEYS; i++) stack_node.keys[i]=0;
	for(int i=0; i<=MAX_KEYS; i++) stack_node.children[i]=0;
	
	int page = alloc_page(disk);
	
	stack_node.block_number = page;
	
	BTreeNode *mmap_node = (BTreeNode*)get_block(disk, page);
	
	memcpy((char*)mmap_node, (char*)&stack_node, sizeof(BTreeNode));
	
	return mmap_node;
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

uint64_t btree_search(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *node = (BTreeNode*)get_block(disk, root_block);
	
	while (node != NULL) {
		if (node->is_leaf) {
			if (node->key == key) {
				printf("Found key!\n");
				return node->block_number;
			} else {
				printf("Did not find key!\n");
				return -1;
			}
		} else {
			int i = 0;
			for (i=0; i < node->num_keys && key > node->keys[i]; i++);
			
			if (node->children[i] != 0) {
				node = (BTreeNode*)get_block(disk, node->children[i]);
			} else {
				printf("Did not find key!\n");
				return -1;
			}
		}
	}
	
	printf("Did not find key!\n");
	return -1;
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

int btree_insert_nonfull(BTreeNode *root, BTreeNode *node)
{
	int i;
	for(i=0; i<root->num_keys && root->keys[i] < node->key; i++);
	
	for(int j=root->num_keys; j>i; j--) {
		root->keys[j] = root->keys[j-1];
		root->children[j] = root->children[j-1];
	}
	
	root->keys[i] = node->key;
	root->children[i] = node->block_number;
	node->parent = root->block_number;
	root->num_keys++;
	
	printf("Placing node with key %lu at position %d\n", node->key, i);
	printf("Block number = %lu\n", node->block_number);
	
	return 0;
}

int btree_insertion_search(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	
	int depth=0;
	while (depth<=btree_find_height(disk, root_block))
	{
		int i;
		for(i = 0; i < root->num_keys && root->keys[i] < key; i++);
		
		if (root->children[i] != 0) {
			root = get_block(disk, root->children[i]);
		} else if (depth==btree_find_height(disk, root_block)-1) {
			root = get_block(disk, root->children[i-1]);
		}
		depth++;
	}
	return (root->parent==0) ? root->block_number : root->parent;
	/*while (!root->is_leaf)
	{
		if (root->num_keys == 0) return root_block;
		
		int i;
		for(i = 0; i < root->num_keys && root->keys[i] < key; i++);
		
		if (root->children[i] != 0) {
			root = get_block(disk, root->children[i]);
		} else {
			return root_block;
		}
	}
	
	return (root->parent==0) ? root->block_number : root->parent;*/
}

void btree_update_parent_keys(DiskInterface* disk, BTreeNode* node)
{
	if (node->parent == 0) return;
	
	BTreeNode *parent = (BTreeNode*)get_block(disk, node->parent);
	
	int child_index = -1;
	for (int i = 0; i <= parent->num_keys; i++) {
		if (parent->children[i] == node->block_number) {
			child_index = i;
			break;
		}
	}
	
	if (child_index == -1) return;
	
	uint64_t min_key;
	if (node->is_leaf) {
		min_key = node->key;
	} else {
		min_key = btree_find_minimum(disk, node->block_number);
	}
		
	parent->keys[child_index - 1] = min_key;
	
	btree_update_parent_keys(disk, parent);
}

int btree_insert(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	BTreeNode *node = btree_node_create(disk, true);
	node->key = key;
	
	int target_block = btree_insertion_search(disk, root_block, key);
	BTreeNode *target = (BTreeNode*)get_block(disk, target_block);
	
	if (target->num_keys == MAX_KEYS) {
		if (key > target->keys[MAX_KEYS-1] && target->children[MAX_KEYS]==0)
		{
			target->children[MAX_KEYS] = key;
		}
		else
		{
			if (target->parent!=0) {
				BTreeNode *parent = (BTreeNode*)get_block(disk, target->parent);
				int i;
				for(i=0; i<MAX_KEYS && parent->keys[i] < btree_find_minimum(disk, target->block_number) && parent->keys[i]!=0; i++);
				btree_split_node(disk, parent, i, target);
				target_block = btree_insertion_search(disk, root_block, key);
				target = (BTreeNode*)get_block(disk, target_block);
			}
			else
			{
				btree_split_root(disk, target);
				target_block = btree_insertion_search(disk, root_block, key);
				target = (BTreeNode*)get_block(disk, target_block);
			}
			btree_insert_nonfull(target, node);
		}
	}
	else btree_insert_nonfull(target, node);
}

void btree_remove_key(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	BTreeNode *first_child = (BTreeNode*)get_block(disk, root->children[0]);
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i] < key && root->keys[i]!=0; i++);
	// TODO: Merge children if num_keys < MIN_KEYS
	if (root->keys[i] == key)
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
	}
	else if (i==0)
	{
		root->children[0] = 0;
		root->num_keys--;
	}
	
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
		child_a->num_keys++;
	}
	
	for (int i = MIN_KEYS; i < root->num_keys; i++) {
		child_b->keys[i - MIN_KEYS] = root->keys[i];
		child_b->num_keys++;
	}
	
	if (!root->is_leaf) {
		for (int i = 0; i <= MIN_KEYS; i++) {
			child_a->children[i] = root->children[i];
			if (root->children[i] != 0) {
				BTreeNode *grandchild = (BTreeNode*)get_block(disk, root->children[i]);
				grandchild->parent = child_a->block_number;
			}
		}
		for (int i = MIN_KEYS + 1; i <= root->num_keys; i++) {
			child_b->children[i - MIN_KEYS - 1] = root->children[i];
			if (root->children[i] != 0) {
				BTreeNode *grandchild = (BTreeNode*)get_block(disk, root->children[i]);
				grandchild->parent = child_b->block_number;
			}
		}
	}
	
	root->is_leaf = false;
	root->num_keys = 1;
	root->keys[0] = root->keys[MIN_KEYS];
	root->children[0] = child_a->block_number;
	root->children[1] = child_b->block_number;
	
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
	BTreeNode *current_parent = node;
	BTreeNode *current_child = child;
	int current_index = index;
	
	while (true) {
		BTreeNode *child_b = btree_node_create(disk, current_child->is_leaf);
		child_b->parent = current_parent->block_number;
		
		uint64_t promoted_key = current_child->keys[MIN_KEYS];
		
		int child_b_index = 0;
		for (int i = MIN_KEYS + 1; i < current_child->num_keys; i++) {
			child_b->keys[child_b_index] = current_child->keys[i];
			current_child->keys[i] = 0;
			child_b->num_keys++;
			child_b_index++;
		}
		
		if (!current_child->is_leaf) {
			child_b_index = 0;
			for (int i = MIN_KEYS + 1; i <= current_child->num_keys; i++) {
				child_b->children[child_b_index] = current_child->children[i];
				current_child->children[i] = 0;
				if (child_b->children[child_b_index] != 0) {
					BTreeNode *grandchild = (BTreeNode*)get_block(disk, child_b->children[child_b_index]);
					grandchild->parent = child_b->block_number;
				}
				child_b_index++;
			}
		}
		
		current_child->keys[MIN_KEYS] = 0;
		current_child->num_keys = MIN_KEYS;
		
		if (current_parent->num_keys < MAX_KEYS) {
			for (int i = current_parent->num_keys; i > current_index; i--) {
				current_parent->keys[i] = current_parent->keys[i - 1];
				current_parent->children[i + 1] = current_parent->children[i];
			}
			
			current_parent->keys[current_index] = promoted_key;
			current_parent->children[current_index + 1] = child_b->block_number;
			current_parent->num_keys++;
			break;
		}
		
		if (current_parent->parent == 0) {
			btree_split_root(disk, current_parent);
			
			for (int i = current_parent->num_keys; i > current_index; i--) {
				current_parent->keys[i] = current_parent->keys[i - 1];
				current_parent->children[i + 1] = current_parent->children[i];
			}
			
			current_parent->keys[current_index] = promoted_key;
			current_parent->children[current_index + 1] = child_b->block_number;
			current_parent->num_keys++;
			break;
		}
		
		BTreeNode *grandparent = (BTreeNode*)get_block(disk, current_parent->parent);
		int parent_index;
		for (parent_index = 0; parent_index <= grandparent->num_keys; parent_index++) {
			if (grandparent->children[parent_index] == current_parent->block_number) break;
		}
		
		for (int i = current_parent->num_keys; i > current_index; i--) {
			current_parent->keys[i] = current_parent->keys[i - 1];
			current_parent->children[i + 1] = current_parent->children[i];
		}
		
		current_parent->keys[current_index] = promoted_key;
		current_parent->children[current_index + 1] = child_b->block_number;
		current_parent->num_keys++;
		
		current_child = current_parent;
		current_parent = grandparent;
		current_index = parent_index;
	}
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
	
	/*printf("hash('/a')=%d\n", hash("/a"));
	btree_insert(disk, root->block_number, hash("/a"));
	btree_insert(disk, root->block_number, hash("/b"));
	debug_print_node(disk, root->block_number, 1);
	btree_insert(disk, root->block_number, hash("/"));
	//btree_insert(disk, root->block_number, hash("/c"));
	btree_insert(disk, root->block_number, hash("/d"));
	//btree_insert(disk, root->block_number, hash("/e"));
	btree_search(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/b"));
	debug_print_node(disk, root->block_number, 1);
	btree_search(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/b"));
	btree_search(disk, root->block_number, hash("/f"));
	//btree_delete(disk, root->block_number, hash("/e"));
	//btree_search(disk, root->block_number, hash("/e"));
	btree_search(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/d"));
	btree_insert(disk, root->block_number, hash("/f"));
	debug_print_node(disk, root->block_number, 1);
	//btree_delete(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/a"));
	//btree_search(disk, root->block_number, hash("/f"));*/
	
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
