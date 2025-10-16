#!/usr/bin/env python3
import pexpect
import random
import sys
import time
import re

class BTreeTester:
    def __init__(self):
        self.child = None
        self.tree_structure = {}
        
    def start_btree(self):
        """Start the btree program"""
        try:
            print("Starting btree program...")
            self.child = pexpect.spawn('./btree', timeout=5)
            # Uncomment to see all output for debugging
            self.child.logfile_read = sys.stdout.buffer
            
            # Wait for the initial prompt - use more flexible pattern
            self.child.expect('Select 1 to insert')
            print("B-tree program started successfully")
            return True
        except Exception as e:
            print(f"Error starting btree: {e}")
            return False
    
    def wait_for_prompt(self):
        """Wait for the main menu prompt"""
        try:
            self.child.expect('Select 1 to insert', timeout=2)
            return True
        except:
            return False
    
    def insert_key(self, key):
        """Insert a single key into the B-tree"""
        try:
            self.child.sendline('1')
            self.child.expect('Key to insert:', timeout=2)
            self.child.sendline(str(key))
            return self.wait_for_prompt()
        except Exception as e:
            print(f"Error inserting key {key}: {e}")
            return False
    
    def get_tree_structure(self):
        """Get the current tree structure and parse it"""
        try:
            # Clear any existing output
            try:
                self.child.read_nonblocking(size=4096, timeout=0.1)
            except:
                pass
            
            self.child.sendline('3')  # Send debug print command
            time.sleep(1)  # Give more time for tree output
            
            # Capture the tree output - look for the tree structure
            tree_output = ""
            start_time = time.time()
            
            while time.time() - start_time < 3:  # Max 3 seconds to capture
                try:
                    chunk = self.child.read_nonblocking(size=1024, timeout=0.5)
                    tree_output += chunk.decode('utf-8', errors='ignore')
                    
                    # If we see the menu prompt, we're done
                    if 'Select 1 to insert' in tree_output:
                        break
                except pexpect.TIMEOUT:
                    continue
                except pexpect.EOF:
                    break
                except Exception:
                    break
            
            # Parse the tree structure
            if tree_output:
                self.parse_tree_output(tree_output)
            
            # Make sure we're back at the prompt
            self.wait_for_prompt()
            return tree_output
            
        except Exception as e:
            print(f"Error getting tree structure: {e}")
            return ""
    
    def parse_tree_output(self, output):
        """Parse the tree output to build a structure representation"""
        self.tree_structure = {
            'nodes': {},
            'leaves': [],
            'internal_nodes': {},
            'parent_children': {}  # parent -> list of children
        }
        
        # Regex patterns to match node lines
        leaf_pattern = r'Block (\d+): LEAF key=(\d+) parent=(\d+)'
        internal_pattern = r'Block (\d+): INTERNAL keys=\[([^\]]*)\] children=\[([^\]]*)\]'
        
        lines = output.split('\n')
        for line in lines:
            # Match leaf nodes
            leaf_match = re.search(leaf_pattern, line)
            if leaf_match:
                block_num, key, parent = leaf_match.groups()
                self.tree_structure['nodes'][block_num] = {
                    'type': 'leaf',
                    'key': int(key),
                    'parent': parent,
                    'block_num': block_num
                }
                self.tree_structure['leaves'].append(block_num)
                continue
            
            # Match internal nodes
            internal_match = re.search(internal_pattern, line)
            if internal_match:
                block_num, keys_str, children_str = internal_match.groups()
                # Handle empty keys and children arrays
                keys = []
                if keys_str and keys_str.strip():
                    keys = [int(k) for k in keys_str.split(',') if k.strip()]
                children = []
                if children_str and children_str.strip():
                    children = [c.strip() for c in children_str.split(',') if c.strip() and c != '0']
                
                self.tree_structure['nodes'][block_num] = {
                    'type': 'internal',
                    'keys': keys,
                    'children': children,
                    'block_num': block_num,
                    'num_children': len(children)
                }
                self.tree_structure['internal_nodes'][block_num] = self.tree_structure['nodes'][block_num]
                
                # Build parent-children relationships
                for child in children:
                    if child and child != '0':
                        if block_num not in self.tree_structure['parent_children']:
                            self.tree_structure['parent_children'][block_num] = []
                        self.tree_structure['parent_children'][block_num].append(child)
        
        print(f"Parsed tree: {len(self.tree_structure['nodes'])} total nodes, "
              f"{len(self.tree_structure['leaves'])} leaves, "
              f"{len(self.tree_structure['internal_nodes'])} internal nodes")
        
        # Debug: print what we found
        if not self.tree_structure['nodes']:
            print("Warning: No nodes parsed from tree output")
            print("Sample of output received:")
            for line in output.split('\n')[:10]:
                if line.strip():
                    print(f"  {line}")
    
    def find_test_candidates(self):
        """
        Find good candidates for deletion testing
        """
        candidates = {
            'sibling_leaves': [],  # Lists of leaf siblings
            'parent_with_multiple_children': []  # Parents with multiple children
        }
        
        # Find parents with multiple leaf children
        for parent, children in self.tree_structure['parent_children'].items():
            leaf_children = []
            for child in children:
                if child in self.tree_structure['nodes'] and self.tree_structure['nodes'][child]['type'] == 'leaf':
                    leaf_children.append(child)
            
            if len(leaf_children) >= 2:
                candidates['sibling_leaves'].append({
                    'parent': parent,
                    'leaves': leaf_children,
                    'parent_info': self.tree_structure['nodes'].get(parent, {})
                })
        
        # Find internal nodes with multiple children
        for block_num, node in self.tree_structure['internal_nodes'].items():
            if node['num_children'] >= 2:
                candidates['parent_with_multiple_children'].append({
                    'parent': block_num,
                    'num_children': node['num_children'],
                    'node_info': node
                })
        
        return candidates
    
    def delete_key(self, key):
        """Delete a key from the B-tree"""
        try:
            self.child.sendline('4')
            self.child.expect('Key to delete:', timeout=2)
            self.child.sendline(str(key))
            result = self.wait_for_prompt()
            if result:
                print(f"Successfully deleted key: {key}")
            return result
        except Exception as e:
            print(f"Error deleting key {key}: {e}")
            return False
    
    def run_test_sequence(self, num_keys):
        """Run the complete test sequence"""
        if not self.start_btree():
            print("Failed to start B-tree program")
            return
        
        # Insert random keys
        keys = list(set([random.randint(1, 1000) for _ in range(num_keys)]))
        print(f"Inserting {len(keys)} unique random keys...")
        
        successful_inserts = 0
        for i, key in enumerate(keys):
            if i % 20 == 0:
                print(f"Progress: {i}/{len(keys)}")
            if self.insert_key(key):
                successful_inserts += 1
        
        print(f"Successfully inserted {successful_inserts} keys")
        
        # Get initial tree structure
        print("Getting initial tree structure...")
        tree_output = self.get_tree_structure()
        
        if not self.tree_structure['nodes']:
            print("ERROR: Could not parse tree structure. The tree might be empty or parsing failed.")
            print("Raw output sample:")
            print(tree_output[:500])
            return
        
        # Find deletion candidates
        candidates = self.find_test_candidates()
        
        print("\n" + "="*60)
        print("DELETION TEST CANDIDATES FOUND:")
        print("="*60)
        
        # Display sibling leaf candidates
        if candidates['sibling_leaves']:
            print("\n1. SIBLING LEAVES (for testing borrowing between siblings):")
            for i, group in enumerate(candidates['sibling_leaves'][:3]):
                print(f"   Group {i+1}:")
                print(f"     Parent: Block {group['parent']}")
                print(f"     Leaf children: {len(group['leaves'])}")
                leaf_keys = []
                for leaf in group['leaves']:
                    leaf_info = self.tree_structure['nodes'][leaf]
                    leaf_keys.append(leaf_info['key'])
                    print(f"       - Block {leaf}: key={leaf_info['key']}")
                print(f"     Leaf keys: {leaf_keys}")
        else:
            print("\nNo sibling leaf groups found (need parents with multiple leaf children)")
        
        # Select specific test cases
        test_cases = self.select_test_cases(candidates)
        
        if test_cases:
            print("\n" + "="*60)
            print("EXECUTING DELETION TESTS:")
            print("="*60)
            
            # Run deletion tests
            self.execute_deletion_tests(test_cases)
        else:
            print("\nNo suitable test cases found. Tree might be too small or simple.")
            print("Try with more keys (30+) for better test coverage.")
            
            # Fallback: delete some random leaves anyway
            if self.tree_structure['leaves']:
                print("\nPerforming fallback random deletion test...")
                random_leaves = random.sample(self.tree_structure['leaves'], min(2, len(self.tree_structure['leaves'])))
                for leaf in random_leaves:
                    key = self.tree_structure['nodes'][leaf]['key']
                    print(f"Deleting random leaf with key {key}")
                    self.delete_key(key)
                    time.sleep(0.5)
        
        # Show final tree state
        print("\nFinal tree structure after deletions:")
        self.get_tree_structure()
        
        # Exit
        print("\nTest completed. Exiting...")
        self.child.sendline('5')  # Exit
        try:
            self.child.expect(pexpect.EOF, timeout=2)
        except:
            pass
    
    def select_test_cases(self, candidates):
        """Select specific test cases for deletion"""
        test_cases = []
        
        # Test case 1: Delete sibling leaves to test borrowing
        if candidates['sibling_leaves']:
            for group in candidates['sibling_leaves']:
                if len(group['leaves']) >= 3:
                    # Delete middle leaf to test borrowing
                    middle_leaf = group['leaves'][1]
                    test_cases.append({
                        'type': 'sibling_borrowing',
                        'description': 'Delete middle leaf to test borrowing from siblings',
                        'keys_to_delete': [self.tree_structure['nodes'][middle_leaf]['key']],
                        'expected_behavior': 'Should borrow from left or right sibling'
                    })
                    break
        
        # Test case 2: Delete multiple leaves from same parent to test merging
        if candidates['sibling_leaves']:
            for group in candidates['sibling_leaves']:
                if len(group['leaves']) >= 2:
                    leaf_keys = [self.tree_structure['nodes'][leaf]['key'] for leaf in group['leaves'][:2]]
                    test_cases.append({
                        'type': 'merge_test',
                        'description': f'Delete {len(leaf_keys)} leaves from same parent to potentially trigger merge',
                        'keys_to_delete': leaf_keys,
                        'expected_behavior': 'May trigger node merging after deletion'
                    })
                    break
        
        # Test case 3: Random deletion for general testing
        if self.tree_structure['leaves'] and len(test_cases) < 2:
            random_leaf = random.choice(self.tree_structure['leaves'])
            random_key = self.tree_structure['nodes'][random_leaf]['key']
            test_cases.append({
                'type': 'random_deletion',
                'description': 'Random leaf deletion',
                'keys_to_delete': [random_key],
                'expected_behavior': 'General deletion test'
            })
        
        return test_cases
    
    def execute_deletion_tests(self, test_cases):
        """Execute the selected deletion test cases"""
        for i, test_case in enumerate(test_cases):
            print(f"\n--- Test Case {i+1}: {test_case['type']} ---")
            print(f"Description: {test_case['description']}")
            print(f"Expected: {test_case['expected_behavior']}")
            print(f"Deleting keys: {test_case['keys_to_delete']}")
            
            # Get tree before deletion
            print("\nTree before deletion:")
            self.get_tree_structure()
            
            # Perform deletions
            all_success = True
            for key in test_case['keys_to_delete']:
                if not self.delete_key(key):
                    all_success = False
                time.sleep(0.5)  # Small delay between deletions
            
            # Get tree after deletion
            print("\nTree after deletion:")
            self.get_tree_structure()
            
            if all_success:
                print(f"✓ Test Case {i+1} Completed Successfully")
            else:
                print(f"✗ Test Case {i+1} Had Some Failures")
            
            print(f"--- Test Case {i+1} Finished ---\n")
            
            time.sleep(1)  # Delay between test cases

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 btree_deletion_test.py <number_of_keys>")
        print("Recommended: 30-50 keys for good tree structure")
        sys.exit(1)
    
    try:
        num_keys = int(sys.argv[1])
        if num_keys < 10:
            print("Please provide at least 10 keys for meaningful tree structure")
            sys.exit(1)
            
        tester = BTreeTester()
        tester.run_test_sequence(num_keys)
        
    except ValueError:
        print("Please provide a valid number")
    except KeyboardInterrupt:
        print("\nTest interrupted by user")
    except Exception as e:
        print(f"Unexpected error: {e}")

if __name__ == "__main__":
    main()
