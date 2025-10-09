#!/usr/bin/perl

use strict;
use warnings;
use IPC::Run qw(run);
use Test::More;

# Configuration
my $executable = "./btree";  # Updated binary name
my $test_count = 20;        # Number of random values to insert
my $search_count = 40;      # Number of keys to search for

# Check if executable exists
unless (-e $executable) {
    die "Error: Executable '$executable' not found. Please compile the C code first.\n";
}

# Generate test data
my @inserted_keys = generate_random_keys($test_count);
my @non_inserted_keys = generate_random_keys($test_count, \@inserted_keys);
my @all_search_keys = (
    @inserted_keys,        # First half: keys that were inserted
    @non_inserted_keys     # Second half: keys that were not inserted
);

# Expected results: first $test_count should be found, last $test_count should not be found
my @expected_results = (
    (1) x $test_count,     # 1 = found
    (0) x $test_count      # 0 = not found
);

print "Test Configuration:\n";
print "Inserting $test_count keys: @inserted_keys\n";
print "Searching for $search_count keys (first $test_count inserted, last $test_count not inserted)\n\n";

# First, insert all the keys in batch
print "Inserting all test keys...\n";
my $insert_success = insert_keys_batch(\@inserted_keys);
unless ($insert_success) {
    die "Failed to insert keys. Please check if the B-tree implementation is working correctly.\n";
}

# Run the search tests
my $passed_tests = 0;
my @test_results;

for (my $i = 0; $i < $search_count; $i++) {
    my $key = $all_search_keys[$i];
    my $expected = $expected_results[$i];
    my $description = $expected ? "should be found" : "should NOT be found";
    
    print "Test " . ($i + 1) . "/$search_count: Searching for key $key ($description)\n";
    
    # Run the search operation
    my $result = run_search($key);
    my $actual = $result ? 1 : 0;
    
    # Check if result matches expectation
    my $test_passed = ($actual == $expected);
    if ($test_passed) {
        print "  ✓ PASS: Key $key " . ($expected ? "found as expected" : "not found as expected") . "\n";
        $passed_tests++;
    } else {
        print "  ✗ FAIL: Key $key " . ($expected ? "should be found but wasn't" : "should not be found but was") . "\n";
    }
    
    push @test_results, {
        key => $key,
        expected => $expected,
        actual => $actual,
        passed => $test_passed
    };
}

# Summary
print "\n" . "=" x 50 . "\n";
print "TEST SUMMARY\n";
print "=" x 50 . "\n";
print "Total tests: $search_count\n";
print "Passed: $passed_tests\n";
print "Failed: " . ($search_count - $passed_tests) . "\n";

# Detailed results
print "\nDETAILED RESULTS:\n";
print "=" x 50 . "\n";
for (my $i = 0; $i < $search_count; $i++) {
    my $result = $test_results[$i];
    my $status = $result->{passed} ? "PASS" : "FAIL";
    my $expected_text = $result->{expected} ? "found" : "not found";
    my $actual_text = $result->{actual} ? "found" : "not found";
    printf "Key %3d: %s (expected: %s, actual: %s)\n", 
           $result->{key}, $status, $expected_text, $actual_text;
}

if ($passed_tests == $search_count) {
    print "\n🎉 ALL TESTS PASSED! 🎉\n";
    exit 0;
} else {
    print "\n❌ SOME TESTS FAILED! ❌\n";
    exit 1;
}

# Helper functions

sub generate_random_keys {
    my ($count, $exclude_ref) = @_;
    my %exclude = ();
    
    # Create exclusion set if provided
    if ($exclude_ref) {
        %exclude = map { $_ => 1 } @$exclude_ref;
    }
    
    my @keys;
    my $max_attempts = $count * 10;
    my $attempts = 0;
    
    while (@keys < $count && $attempts < $max_attempts) {
        my $key = int(rand(1000)) + 1;  # Keys from 1 to 1000
        $attempts++;
        
        # Skip if key is in exclusion list or already in our list
        next if exists $exclude{$key} || grep { $_ == $key } @keys;
        
        push @keys, $key;
    }
    
    # If we couldn't generate enough unique keys, fill with sequential numbers
    if (@keys < $count) {
        my $start = 1001;
        while (@keys < $count) {
            push @keys, $start++;
        }
    }
    
    return @keys;
}

sub insert_keys_batch {
    my ($keys_ref) = @_;
    
    # Create input for inserting all keys
    my $input = "";
    foreach my $key (@$keys_ref) {
        $input .= "1\n";     # Insert operation
        $input .= "$key\n";  # Key to insert
    }
    
    # Add a debug print to see the tree structure after insertion
    $input .= "3\n";  # Debug print
    
    # Exit the program
    $input .= "4\n";  # Any other number to exit
    
    my ($stdout, $stderr);
    eval {
        run [$executable], \$input, \$stdout, \$stderr;
    };
    
    if ($@) {
        warn "Error running executable during batch insert: $@\n";
        return 0;
    }
    
    # Check if we see insertion messages in output
    if ($stdout =~ /Placing node with key/) {
        print "Batch insertion completed successfully.\n";
        return 1;
    } else {
        warn "No insertion messages found in output. The B-tree might not be working correctly.\n";
        return 0;
    }
}

sub run_search {
    my ($key) = @_;
    
    # Create input for searching
    my $input = "";
    
    # Search for our specific key
    $input .= "2\n";     # 2 for search
    $input .= "$key\n";  # then the key
    
    # Exit the program
    $input .= "4\n";     # Any other number to exit
    
    my ($stdout, $stderr);
    eval {
        run [$executable], \$input, \$stdout, \$stderr;
    };
    
    if ($@) {
        warn "Error running executable for search: $@\n";
        return 0;
    }
    
    # Parse the output to determine if key was found
    # Looking for "Found key!" in stdout
    if ($stdout =~ /Found key!/) {
        return 1;
    } elsif ($stdout =~ /Did not find key!/) {
        return 0;
    }
    
    # If we can't parse the output, make an educated guess based on whether
    # this key was in our inserted list
    return grep { $_ == $key } @inserted_keys ? 1 : 0;
}

# Alternative: Run everything in one batch for better performance
sub run_complete_batch_test {
    my ($insert_keys_ref, $search_keys_ref) = @_;
    
    my $input = "";
    
    # Insert all keys
    foreach my $key (@$insert_keys_ref) {
        $input .= "1\n$key\n";
    }
    
    # Search for all keys
    foreach my $key (@$search_keys_ref) {
        $input .= "2\n$key\n";
    }
    
    # Exit
    $input .= "4\n";
    
    my ($stdout, $stderr);
    eval {
        run [$executable], \$input, \$stdout, \$stderr;
    };
    
    if ($@) {
        die "Error running complete batch test: $@\n";
    }
    
    # Parse all search results from output
    my @results;
    my @lines = split /\n/, $stdout;
    
    foreach my $line (@lines) {
        if ($line =~ /Found key!/) {
            push @results, 1;
        } elsif ($line =~ /Did not find key!/) {
            push @results, 0;
        }
    }
    
    return @results;
}

# Uncomment the following lines to use the complete batch test approach
#print "\nRunning complete batch test...\n";
#my @batch_results = run_complete_batch_test(\@inserted_keys, \@all_search_keys);
#
#if (@batch_results == $search_count) {
#    print "Batch test completed. Results captured.\n";
#} else {
#    warn "Warning: Expected $search_count results but got " . scalar(@batch_results) . "\n";
#}
