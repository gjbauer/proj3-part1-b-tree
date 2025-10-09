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
	int rv = -1;
	//printf("Searching for key %lu\n", key);
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i]!=0; i++)
	{
		if (root->keys[i] >= key) break;
	}
	
	if (root->key == key) {
		printf("Found key!\n");
		rv = root->block_number;
	}
	else if (root->keys[i] == key) {
		return btree_search(disk, root->children[i+1], key);
	}
	else if (root->keys[i] > key && root->children[i]!=0)
	{
		return btree_search(disk, root->children[i], key);
	}
	else printf("Did not find key!\n");
	
	return rv;
}

int btree_find_height(DiskInterface* disk, uint64_t node_block)
{
	BTreeNode *node = (BTreeNode*)get_block(disk, node_block);
	
	int height=0;
	while (node->parent!=0)
	{
		node = (BTreeNode*)get_block(disk, node->parent);
		height++;
	}
	return height;
}

int btree_find_minimum(DiskInterface* disk, uint64_t root_block)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	
	if (root->children[0]!=0)
	{
		BTreeNode *first_child = (BTreeNode*)get_block(disk, root->children[0]);
		if (first_child->is_leaf) return first_child->key;
		else return btree_find_minimum(disk, first_child->block_number);
	}
	else return root->keys[0];
}

int btree_insert_nonfull(BTreeNode *root, BTreeNode *node)
{
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
	
	return 0;
}

int btree_insertion_search(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	
	int i;
	for(i=0; i<MAX_KEYS && root->keys[i]!=0; i++)
	{
		if (root->keys[i] >= key) break;
	}
	
	if (root->is_leaf) 
	{
		printf("returning %lu\n", root->parent);
		return root->parent;
	}
	
	uint64_t child = 0;
	if (i < MAX_KEYS && root->keys[i] > key)
	{
		child = root->children[i];
	}
	else if (i < MAX_KEYS)
	{
		child = root->children[i+1];
	}
	
	if (child == 0) {
		return root->block_number;
	}
	
	return btree_insertion_search(disk, child, key);
}

int btree_insert(DiskInterface* disk, uint64_t root_block, uint64_t key)
{
	BTreeNode *root = (BTreeNode*)get_block(disk, root_block);
	BTreeNode *node = btree_node_create(disk, true);
	node->key = key;
	
	if (root->num_keys > 0)
	{
		int target_block = btree_insertion_search(disk, root_block, key);
		BTreeNode *target = (BTreeNode*)get_block(disk, target_block);
		
		if (target->num_keys == MAX_KEYS) {
			if (key > target->keys[0] && target->children[0]==0)
			{
				for (int i=0; i<MAX_KEYS-1; i++)
				{
					target->keys[i] = target->keys[i+1];
				}
				target->keys[MAX_KEYS-1] = 0;
				for (int i=0; i<MAX_KEYS; i++)
				{
					target->children[i] = target->children[i+1];
				}
				target->children[MAX_KEYS] = 0;
				target->num_keys--;
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
			}
		}
		btree_insert_nonfull(target, node);
	}
	else
	{
		printf("Placing node with key %lu at position %d\n", key, 1);
		printf("Block number = %lu\n", node->block_number);
		root->keys[0] = key;
		root->children[1] = node->block_number;
		node->parent=root->block_number;
		root->num_keys++;
	}
	
	return 0;
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
	BTreeNode *child_b = btree_node_create(disk, child->is_leaf);
	child_b->parent = node->block_number;
	
	for (int i = MIN_KEYS; i < child->num_keys; i++) {
		child_b->keys[i - MIN_KEYS] = child->keys[i];
		child->keys[i] = 0;
		child_b->num_keys++;
	}
	
	if (!child->is_leaf) {
		for (int i = MIN_KEYS + 1; i <= child->num_keys; i++) {
			child_b->children[i - MIN_KEYS] = child->children[i];
			child->children[i] = 0;
			if (child_b->children[i - MIN_KEYS] != 0) {
				BTreeNode *grandchild = (BTreeNode*)get_block(disk, child_b->children[i - MIN_KEYS]);
				grandchild->parent = child_b->block_number;
			}
		}
	}
	
	child->num_keys = MIN_KEYS;
	
	uint64_t promoted_key = btree_find_minimum(disk, child->block_number);
	child->num_keys--;
	
	if (node->num_keys < MAX_KEYS) {
		for (int i = node->num_keys; i > index; i--) {
			node->keys[i] = node->keys[i - 1];
			node->children[i + 1] = node->children[i];
		}
		
		node->keys[index] = promoted_key;
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
		} else {
			btree_split_root(disk, node);
		}
		btree_split_node(disk, node, index, child);
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
	
	btree_insert(disk, root->block_number, hash("/a"));
	btree_insert(disk, root->block_number, hash("/b"));
	btree_insert(disk, root->block_number, hash("/"));
	btree_insert(disk, root->block_number, hash("/c"));
	btree_insert(disk, root->block_number, hash("/d"));
	btree_insert(disk, root->block_number, hash("/e"));
	debug_print_node(disk, root->block_number, 1);
	/*btree_search(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/b"));
	btree_search(disk, root->block_number, hash("/f"));
	btree_delete(disk, root->block_number, hash("/e"));
	btree_search(disk, root->block_number, hash("/e"));
	btree_search(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/d"));
	btree_insert(disk, root->block_number, hash("/f"));
	btree_delete(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/a"));
	btree_search(disk, root->block_number, hash("/f"));*/
	
	/*while (true) {
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
	}*/
}
