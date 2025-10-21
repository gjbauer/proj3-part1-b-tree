## B-Tree Design and Algorithm Implementation

### Introduction

B-Trees were first introduced in 1970 by Rudolf Bayer and Edward M. McCreight. They provide a data structure capable of time-efficient operations with a time complexity of O(log n) for insertion, search, and deletion, and O(n) time for traversal. This is why they are ubitquitous in database management systems and have become an ever increasingly important data structure for modern filesystems. This document provides an introduction to the design architecture and algorithmic implementation to the B-Tree variant implemented and used in this filesystem.

#### B-Tree Structure

The nodes in the B-Tree have the following structure:

```
struct BTreeNode {
    uint64_t block_number;		// Physical block number on disk where this node is stored
    bool is_leaf;			// Whether this is a leaf node (contains actual data)
    uint64_t key;			// Actual key of node (used when node is leaf)
    uint64_t value;			// Associated value for key-value pairs (B+Tree indexes file and directory inodes)
    uint16_t num_keys;			// Current number of keys stored in this node
    uint64_t keys[MAX_KEYS];		// Array of keys (could be inode numbers or other identifiers)
    uint64_t children[MAX_KEYS + 1];	// Array of child block numbers (internal nodes only)
    uint64_t parent;			// Parent node block number (0 if root)
    uint64_t left_sibling;		// Block number of left sibling (for efficient traversal)
    uint64_t right_sibling;		// Block number of right sibling (for efficient traversal)
}
```

#### B-Tree Operations

This implementation contains the following core operations:

```
btree_search(DiskInterface, root_block, key);
btree_insert(DiskInterface, root_block, key);
btree_delete(DiskInterface, root_block, key);
```

### Algorithm analysis

#### Core B-Tree Operations

##### B-Tree Search

```
btree_search:
	start at root block
	
	if current block is a leaf:
		if the key is equal to the search key:
			print "Found key!"
			return block number
		else;
			return -1
	
	else:
		for i in range of the number of keys:
			if child[i] != 0:
				result = btree_search child[i]
				if result != -1:
					return result
		print "Did not find key!"
		return -1
```
##### B-Tree Insertion

```

int btree_insert_nonfull(DiskInterface* disk, BTreeNode *root, BTreeNode *node)
{
	if (root->is_leaf) {
		printf("ERROR: Trying to insert into leaf node\n");
		return -1;
	} else {
		// Find the correct position for the new child
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
		
		// Shift existing children to make room
		for(int j = root->num_keys; j >= child_pos; j--) {
			root->children[j + 1] = root->children[j];
		}
		
		// Insert the new child
		root->children[child_pos] = node->block_number;
		node->parent = root->block_number;
		root->num_keys++;
		
		// Update keys based on maximum values of children
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
	if (node->parent == 0) return;  // No parent to update
	
	BTreeNode *parent = (BTreeNode*)get_block(disk, node->parent);
	
	// Update all keys in the parent based on the maximum values of each child
	for (int i = 0; i < parent->num_keys; i++) {
		if (parent->children[i] != 0) {
			parent->keys[i] = btree_find_maximum(disk, parent->children[i]);
		}
	}
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
```
##### B-Tree Deletion
```
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

void btree_promote_root(DiskInterface* disk, BTreeNode* root)
{
	int page = root->block_number;
	
	BTreeNode *child = (BTreeNode*)get_block(disk, root->children[0]);
	
	memcpy(root, child, sizeof(BTreeNode));
	
	root->block_number = page;
	root->parent = 0;
	
	btree_node_free(disk, child);
	
	for (int i = 0; i <= root->num_keys; i++) {
		if (root->children[i] != 0) {
			child = (BTreeNode*)get_block(disk, root->children[i]);
			child->parent = root->block_number;
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

int btree_borrow_left(DiskInterface* disk, BTreeNode *node)
{
	int rv = -1;
	if (node->left_sibling==0) return rv;  // No left sibling
	else {
		BTreeNode *left_sibling = (BTreeNode*)get_block(disk, node->left_sibling);
		
		// Can't borrow if sibling has minimum keys
		if (left_sibling->num_keys==MIN_KEYS) return rv;
		else {
			// Find the rightmost child to borrow
			for (int i = left_sibling->num_keys; i >= 0; i--) {
				if (left_sibling->children[i] != 0) {
					if (i < left_sibling->num_keys) left_sibling->keys[i]=0;
					rv = left_sibling->children[i];  // Return borrowed child
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
	if (node->right_sibling==0) return rv;  // No right sibling
	else {
		BTreeNode *right_sibling = (BTreeNode*)get_block(disk, node->right_sibling);
		
		// Can't borrow if sibling has minimum keys
		if (right_sibling->num_keys==MIN_KEYS) return rv;
		else {
			// Borrow the leftmost child
			rv = right_sibling->children[0];
			
			// Shift all keys and children left
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
	if (root->num_keys==MIN_KEYS && root->parent!=0)
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

```

