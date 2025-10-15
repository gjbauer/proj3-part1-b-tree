#!/usr/bin/env python3
import pexpect
import random
import sys
import time

def test_btree_with_pexpect(num_keys):
    """Test B-tree using pexpect with proper tree output capture"""
    try:
        # Start the btree program
        print("Starting btree program...")
        child = pexpect.spawn('./btree', timeout=10)
        child.logfile_read = sys.stdout.buffer  # Print all output to console in real-time
        
        # Wait for the initial prompt
        child.expect('Select 1 to insert')
        print("\nB-tree program started successfully")
        
        # Generate random keys
        keys = [random.randint(1, 1000) for _ in range(num_keys)]
        print(f"Inserting {num_keys} random keys...")
        
        # Insert each key
        for i, key in enumerate(keys):
            if i % 10 == 0:
                print(f"Progress: {i}/{num_keys}")
                
            # Send insert command
            child.sendline('1')
            child.expect('Key to insert:')
            
            # Send the key
            child.sendline(str(key))
            child.expect('Select 1 to insert')
        
        print(f"\nAll {num_keys} keys inserted. Printing tree structure...")
        
        # Request tree print
        child.sendline('3')
        
        # Give it time to generate the tree output
        time.sleep(1)
        
        # Wait for the menu to reappear (indicating tree print is complete)
        child.expect('Select 1 to insert')
        
        # Exit the program
        print("\nExiting B-tree program...")
        child.sendline('4')
        child.expect(pexpect.EOF)
        
    except pexpect.ExceptionPexpect as e:
        print(f"Error: {e}")
    except FileNotFoundError:
        print("Error: 'btree' binary not found in current directory!")
        print("Please compile with: gcc -o btree btr.c disk.c hash.c -I.")

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 btree_insertion_fault.py <number_of_keys>")
        sys.exit(1)
    
    try:
        num_keys = int(sys.argv[1])
        test_btree_with_pexpect(num_keys)
    except ValueError:
        print("Please provide a valid number")
    except KeyboardInterrupt:
        print("\nTest interrupted by user")

if __name__ == "__main__":
    main()
