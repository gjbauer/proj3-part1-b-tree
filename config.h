#ifndef CONFIG_H
#define CONFIG_H

/**
 * Configuration constants for the B-tree filesystem
 * These values control the fundamental parameters of the storage system
 */

// ==================== DISK AND BLOCK CONFIGURATION ====================

/**
 * Size of each disk block in bytes
 * Standard 4KB block size provides good balance between:
 * - Memory usage (smaller blocks use less RAM)
 * - I/O efficiency (larger blocks reduce seek overhead)
 * - Compatibility with most filesystems and storage devices
 */
#define BLOCK_SIZE 4096

// ==================== B-TREE NODE CONFIGURATION ====================

/**
 * Maximum number of keys per B-tree node
 * This value determines the branching factor of the tree:
 * - Higher values = wider, shallower trees (better for sequential access)
 * - Lower values = narrower, deeper trees (better for random access)
 * 
 * Current value of 4 is chosen for:
 * - Easy debugging and visualization
 * - Reasonable performance for small to medium datasets
 * - Fits comfortably within a 4KB block with metadata
 */
#define MAX_KEYS 4

/**
 * Minimum number of keys per B-tree node (except root)
 * Must be at least MAX_KEYS/2 to maintain B-tree balance properties
 * This ensures nodes are at least half-full, providing:
 * - Guaranteed logarithmic height
 * - Efficient space utilization
 * - Predictable performance characteristics
 */
#define MIN_KEYS (MAX_KEYS / 2)

#endif

