#ifndef BTR_H
#define BTR_H
#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "disk.h"

// B-tree node structure
typedef struct BTreeNode {
    uint64_t block_number;		// Physical block number on disk
    bool is_leaf;			// Whether this is a leaf node
    uint64_t key;			// Actual key of node
    uint16_t num_keys;			// Current number of keys
    uint64_t keys[MAX_KEYS];		// Array of keys (could be inode numbers)
    uint64_t children[MAX_KEYS + 1];	// Array of child block numbers
    uint64_t parent;			// Parent node block number
} BTreeNode;

// ==================== B-TREE OPERATIONS ====================

// B-tree core operations
BTreeNode* btree_node_create(DiskInterface* disk, bool is_leaf);
void btree_node_free(BTreeNode* node);
int btree_node_read(DiskInterface* disk, uint64_t block_num, BTreeNode* node);
int btree_node_write(DiskInterface* disk, BTreeNode* node);
uint64_t btree_search(DiskInterface* disk, uint64_t root_block, uint64_t key);
int btree_insert(DiskInterface* disk, uint64_t root_block, uint64_t key);
int btree_delete(DiskInterface* disk, uint64_t* root_block, uint64_t key);
void btree_split_node(DiskInterface* disk, BTreeNode* node, int index);
void btree_merge_children(DiskInterface* disk, BTreeNode* parent, int index);

// B-tree traversal and debugging
void btree_traverse(DiskInterface* disk, uint64_t root_block, void (*callback)(uint64_t key, uint64_t value));
void btree_validate(DiskInterface* disk, uint64_t root_block);
void btree_print(DiskInterface* disk, uint64_t root_block, int level);
#endif

