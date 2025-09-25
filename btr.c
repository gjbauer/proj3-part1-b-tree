#include <stdio.h>
#include <limits.h>
#include "btr.h"

// B-tree core operations
BTreeNode* btree_node_create(bool is_leaf)
{
}

void btree_node_free(BTreeNode* node)
{
}

int btree_node_read(uint64_t block_num, BTreeNode* node)
{
}

int btree_node_write(BTreeNode* node)
{
}

uint64_t btree_search(uint64_t root_block, uint64_t key)
{
}

int btree_insert(uint64_t* root_block, uint64_t key, uint64_t value)
{
}

int btree_delete(uint64_t* root_block, uint64_t key)
{
}

void btree_split_child(BTreeNode* parent, int index, BTreeNode* child)
{
}

void btree_merge_children(BTreeNode* parent, int index)
{
}

// B-tree traversal and debugging
void btree_traverse(uint64_t root_block, void (*callback)(uint64_t key, uint64_t value))
{
}

void btree_validate(uint64_t root_block)
{
}

void btree_print(uint64_t root_block, int level)
{
}

int main()
{
	// TODO: Implement B-Tree operations and begin testing them!!
	printf("UINT_MAX / 256 : %ud\n", UINT_MAX / 256);
}
