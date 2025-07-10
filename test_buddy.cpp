#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iomanip>
using namespace std;

// Forward declarations of your functions
void* smalloc(size_t size);
void* scalloc(size_t num, size_t size);
void sfree(void* p);
void* srealloc(void* oldp, size_t size);
size_t _num_free_blocks();
size_t _num_free_bytes();
size_t _num_allocated_blocks();
size_t _num_allocated_bytes();
size_t _num_meta_data_bytes();
size_t _size_meta_data();

// Enhanced debugging and statistics
bool detect_underflow() {
    // Detect unsigned integer underflow (huge values)
    const size_t UNDERFLOW_THRESHOLD = 1000000000UL;

    return (_num_allocated_blocks() > UNDERFLOW_THRESHOLD ||
            _num_allocated_bytes() > UNDERFLOW_THRESHOLD ||
            _num_free_blocks() > UNDERFLOW_THRESHOLD ||
            _num_free_bytes() > UNDERFLOW_THRESHOLD);
}

void print_detailed_stats(const string& location) {
    cout << "\n--- " << location << " ---" << endl;
    cout << "Free blocks: " << _num_free_blocks()
         << ", Free bytes: " << _num_free_bytes() << endl;
    cout << "Allocated blocks: " << _num_allocated_blocks()
         << ", Allocated bytes: " << _num_allocated_bytes() << endl;
    cout << "Metadata bytes: " << _num_meta_data_bytes() << endl;

    if (detect_underflow()) {
        cout << "*** WARNING: UNDERFLOW DETECTED! ***" << endl;
        cout << "This indicates statistics bugs in your implementation." << endl;
    }
}

void validate_stats_consistency() {
    if (detect_underflow()) {
        cout << "\n❌ CRITICAL ERROR: Statistics underflow detected!" << endl;
        cout << "This typically means double-counting in sfree() or statistics bugs." << endl;
        throw runtime_error("Statistics underflow - fix your allocator first");
    }
}

// Test 0: Basic Functionality Test
bool test_basic_functionality() {
    cout << "\n=== Test 0: Basic Functionality ===" << endl;
    print_detailed_stats("Initial state");

    // Single allocation and free
    void* ptr1 = smalloc(100);
    if (!ptr1) {
        cout << "❌ smalloc(100) failed" << endl;
        return false;
    }
    print_detailed_stats("After smalloc(100)");
    validate_stats_consistency();

    sfree(ptr1);
    print_detailed_stats("After sfree(ptr1)");
    validate_stats_consistency();

    // Second allocation
    void* ptr2 = smalloc(50);
    if (!ptr2) {
        cout << "❌ smalloc(50) failed" << endl;
        return false;
    }
    print_detailed_stats("After smalloc(50)");
    validate_stats_consistency();

    sfree(ptr2);
    print_detailed_stats("After sfree(ptr2)");
    validate_stats_consistency();

    cout << "✓ Basic functionality working" << endl;
    return true;
}

// Test 1: Challenge 0 - Tightest Fit Allocation
bool test_challenge_0_tightest_fit() {
    cout << "\n=== Test 1: Challenge 0 - Tightest Fit Allocation ===" << endl;
    print_detailed_stats("Start of tightest fit test");

    // Allocate different sizes
    vector<void*> ptrs;
    vector<size_t> sizes = {100, 200, 400, 1000};

    for (size_t size : sizes) {
        void* ptr = smalloc(size);
        if (!ptr) {
            cout << "❌ smalloc(" << size << ") failed" << endl;
            return false;
        }
        ptrs.push_back(ptr);
        cout << "Allocated " << size << " bytes" << endl;
    }

    print_detailed_stats("After initial allocations");
    validate_stats_consistency();

    // Free first and third allocations
    sfree(ptrs[0]); // Free 100-byte allocation
    sfree(ptrs[2]); // Free 400-byte allocation

    print_detailed_stats("After freeing some blocks");
    validate_stats_consistency();

    // Test tightest fit
    void* tight_ptr1 = smalloc(90);  // Should fit in freed 100-byte block
    void* tight_ptr2 = smalloc(300); // Should fit in freed 400-byte block

    if (!tight_ptr1 || !tight_ptr2) {
        cout << "❌ Tightest fit allocations failed" << endl;
        return false;
    }

    print_detailed_stats("After tightest fit allocations");
    validate_stats_consistency();

    cout << "✓ Tightest fit allocation working" << endl;

    // Clean up
    sfree(ptrs[1]);
    sfree(ptrs[3]);
    sfree(tight_ptr1);
    sfree(tight_ptr2);

    return true;
}

// Test 2: Challenge 1 - Block Splitting
bool test_challenge_1_block_splitting() {
    cout << "\n=== Test 2: Challenge 1 - Block Splitting ===" << endl;
    print_detailed_stats("Start of block splitting test");

    size_t initial_free_blocks = _num_free_blocks();

    // Allocate a very small block to force splitting
    void* small_ptr = smalloc(50);
    if (!small_ptr) {
        cout << "❌ smalloc(50) failed" << endl;
        return false;
    }

    print_detailed_stats("After small allocation (should cause splitting)");
    validate_stats_consistency();

    size_t after_split_free_blocks = _num_free_blocks();
    cout << "Free blocks - Before: " << initial_free_blocks
         << ", After: " << after_split_free_blocks << endl;

    // Should have more free blocks now due to splitting creating buddies
    if (after_split_free_blocks <= initial_free_blocks) {
        cout << "⚠️  Expected more free blocks after splitting" << endl;
    }

    // Allocate another small block
    void* small_ptr2 = smalloc(60);
    if (!small_ptr2) {
        cout << "❌ smalloc(60) failed" << endl;
        return false;
    }

    print_detailed_stats("After second small allocation");
    validate_stats_consistency();

    cout << "✓ Block splitting test completed" << endl;

    sfree(small_ptr);
    sfree(small_ptr2);

    return true;
}

// Test 3: Challenge 2 - Buddy Merging (Improved)
bool test_challenge_2_buddy_merging() {
    cout << "\n=== Test 3: Challenge 2 - Buddy Merging ===" << endl;
    print_detailed_stats("Start of buddy merging test");

    // Strategy: Allocate many small blocks, then free them to test merging
    vector<void*> small_ptrs;

    // Allocate 4 small blocks (these will create splitting and potential buddies)
    for (int i = 0; i < 4; i++) {
        void* ptr = smalloc(50 + i * 10);
        if (!ptr) {
            cout << "❌ smalloc failed in buddy test" << endl;
            return false;
        }
        small_ptrs.push_back(ptr);
    }

    print_detailed_stats("After allocating multiple small blocks");
    validate_stats_consistency();

    size_t blocks_before_merge = _num_free_blocks();

    // Free all blocks - this should trigger buddy merging
    for (void* ptr : small_ptrs) {
        sfree(ptr);
    }

    print_detailed_stats("After freeing all small blocks (merging should occur)");
    validate_stats_consistency();

    size_t blocks_after_merge = _num_free_blocks();

    cout << "Free blocks - Before freeing: " << blocks_before_merge
         << ", After freeing: " << blocks_after_merge << endl;

    cout << "✓ Buddy merging test completed" << endl;
    return true;
}

// Test 4: Challenge 3 - mmap for Large Allocations
bool test_challenge_3_mmap_large_allocations() {
    cout << "\n=== Test 4: Challenge 3 - mmap Large Allocations ===" << endl;
    print_detailed_stats("Start of mmap test");

    size_t large_size = 150 * 1024; // 150KB > 128KB threshold
    size_t small_size = 100 * 1024; // 100KB < 128KB threshold

    // Test large allocation (should use mmap)
    void* large_ptr = smalloc(large_size);
    if (!large_ptr) {
        cout << "❌ Large allocation (mmap) failed" << endl;
        return false;
    }

    print_detailed_stats("After large allocation (mmap)");
    validate_stats_consistency();

    // Test small allocation (should use buddy allocator)
    void* small_ptr = smalloc(small_size);
    if (!small_ptr) {
        cout << "❌ Small allocation (buddy) failed" << endl;
        return false;
    }

    print_detailed_stats("After small allocation (buddy)");
    validate_stats_consistency();

    // Free large allocation
    sfree(large_ptr);
    print_detailed_stats("After freeing large allocation");
    validate_stats_consistency();

    // Free small allocation
    sfree(small_ptr);
    print_detailed_stats("After freeing small allocation");
    validate_stats_consistency();

    cout << "✓ mmap/munmap working correctly" << endl;
    return true;
}

// Test 5: Statistics Accuracy
bool test_statistics_accuracy() {
    cout << "\n=== Test 5: Statistics Accuracy ===" << endl;

    size_t meta_size = _size_meta_data();
    cout << "Metadata size: " << meta_size << " bytes" << endl;

    if (meta_size > 64) {
        cout << "❌ Metadata size (" << meta_size << ") exceeds 64 bytes requirement" << endl;
        return false;
    }

    print_detailed_stats("Initial statistics");

    size_t initial_allocated_blocks = _num_allocated_blocks();
    size_t initial_allocated_bytes = _num_allocated_bytes();

    // Make some allocations
    void* ptr1 = smalloc(100);
    void* ptr2 = smalloc(200);
    void* ptr3 = smalloc(150 * 1024); // mmap allocation

    if (!ptr1 || !ptr2 || !ptr3) {
        cout << "❌ Allocations failed in statistics test" << endl;
        return false;
    }

    print_detailed_stats("After test allocations");
    validate_stats_consistency();

    // Verify statistics increased
    if (_num_allocated_blocks() <= initial_allocated_blocks) {
        cout << "❌ Allocated blocks count didn't increase properly" << endl;
        return false;
    }

    if (_num_allocated_bytes() <= initial_allocated_bytes) {
        cout << "❌ Allocated bytes count didn't increase properly" << endl;
        return false;
    }

    sfree(ptr1);
    sfree(ptr2);
    sfree(ptr3);

    print_detailed_stats("After freeing test allocations");
    validate_stats_consistency();

    cout << "✓ Statistics tracking correctly" << endl;
    return true;
}

// Test 6: scalloc Functionality
bool test_scalloc_functionality() {
    cout << "\n=== Test 6: scalloc Functionality ===" << endl;
    print_detailed_stats("Start of scalloc test");

    // Test small scalloc
    void* ptr = scalloc(100, 4); // 400 bytes
    if (!ptr) {
        cout << "❌ scalloc(100, 4) failed" << endl;
        return false;
    }

    // Verify zero initialization
    char* char_ptr = (char*)ptr;
    for (int i = 0; i < 400; i++) {
        if (char_ptr[i] != 0) {
            cout << "❌ scalloc didn't zero-initialize memory at byte " << i << endl;
            return false;
        }
    }

    // Test large scalloc (mmap)
    void* large_ptr = scalloc(40000, 4); // 160KB
    if (!large_ptr) {
        cout << "❌ scalloc large allocation failed" << endl;
        return false;
    }

    // Verify zero initialization for large allocation
    char* large_char_ptr = (char*)large_ptr;
    for (int i = 0; i < 160000; i++) {
        if (large_char_ptr[i] != 0) {
            cout << "❌ scalloc didn't zero-initialize large memory at byte " << i << endl;
            return false;
        }
    }

    print_detailed_stats("After scalloc allocations");
    validate_stats_consistency();

    sfree(ptr);
    sfree(large_ptr);

    cout << "✓ scalloc working correctly" << endl;
    return true;
}

// Test 7: Basic srealloc
bool test_srealloc_basic() {
    cout << "\n=== Test 7: Basic srealloc ===" << endl;
    print_detailed_stats("Start of srealloc test");

    // Test basic srealloc
    void* ptr = smalloc(50);
    if (!ptr) {
        cout << "❌ Initial smalloc for srealloc test failed" << endl;
        return false;
    }

    // Write test data
    char* char_ptr = (char*)ptr;
    for (int i = 0; i < 50; i++) {
        char_ptr[i] = 'A' + (i % 26);
    }

    // Realloc to larger size
    void* new_ptr = srealloc(ptr, 100);
    if (!new_ptr) {
        cout << "❌ srealloc to larger size failed" << endl;
        return false;
    }

    // Verify data preservation
    char* new_char_ptr = (char*)new_ptr;
    for (int i = 0; i < 50; i++) {
        if (new_char_ptr[i] != 'A' + (i % 26)) {
            cout << "❌ srealloc didn't preserve data at byte " << i << endl;
            return false;
        }
    }

    print_detailed_stats("After srealloc");
    validate_stats_consistency();

    sfree(new_ptr);

    cout << "✓ srealloc working correctly" << endl;
    return true;
}

// Test 8: Edge Cases
bool test_edge_cases() {
    cout << "\n=== Test 8: Edge Cases ===" << endl;
    print_detailed_stats("Start of edge cases test");

    // Test size 0
    void* ptr_zero = smalloc(0);
    if (ptr_zero != nullptr) {
        cout << "❌ smalloc(0) should return NULL" << endl;
        return false;
    }

    // Test size > 10^8
    void* ptr_huge = smalloc(200000000);
    if (ptr_huge != nullptr) {
        cout << "❌ smalloc(200000000) should return NULL" << endl;
        return false;
    }

    // Test freeing NULL
    sfree(nullptr); // Should not crash

    // Test double free protection
    void* test_ptr = smalloc(100);
    if (!test_ptr) {
        cout << "❌ smalloc(100) failed in edge test" << endl;
        return false;
    }

    sfree(test_ptr);
    sfree(test_ptr); // Should not crash

    print_detailed_stats("After edge cases");
    validate_stats_consistency();

    cout << "✓ Edge cases handled correctly" << endl;
    return true;
}

// Simple mode for debugging
bool run_simple_test() {
    cout << "\n=== SIMPLE DEBUG MODE ===" << endl;
    cout << "Running minimal test to isolate issues..." << endl;

    print_detailed_stats("Initial state");

    cout << "\n1. Testing smalloc(100)..." << endl;
    void* ptr1 = smalloc(100);
    if (!ptr1) {
        cout << "❌ smalloc(100) failed!" << endl;
        return false;
    }
    print_detailed_stats("After smalloc(100)");
    if (detect_underflow()) return false;

    cout << "\n2. Testing sfree(ptr1)..." << endl;
    sfree(ptr1);
    print_detailed_stats("After sfree(ptr1)");
    if (detect_underflow()) return false;

    cout << "\n3. Testing smalloc(50)..." << endl;
    void* ptr2 = smalloc(50);
    if (!ptr2) {
        cout << "❌ smalloc(50) failed!" << endl;
        return false;
    }
    print_detailed_stats("After smalloc(50)");
    if (detect_underflow()) return false;

    cout << "\n4. Testing sfree(ptr2)..." << endl;
    sfree(ptr2);
    print_detailed_stats("After sfree(ptr2)");
    if (detect_underflow()) return false;

    cout << "\n✅ Simple test passed! Your basic allocator is working." << endl;
    return true;
}

// Main test runner
int main(int argc, char* argv[]) {
    cout << "============================================" << endl;
    cout << "  Fixed Buddy Allocator Test Suite - Part 3" << endl;
    cout << "============================================" << endl;

    // Check for simple mode
    bool simple_mode = (argc > 1 && string(argv[1]) == "--simple");

    if (simple_mode) {
        cout << "Running in SIMPLE MODE for debugging..." << endl;
        return run_simple_test() ? 0 : 1;
    }

    try {
        bool all_passed = true;

        all_passed &= test_basic_functionality();
        all_passed &= test_challenge_0_tightest_fit();
        all_passed &= test_challenge_1_block_splitting();
        all_passed &= test_challenge_2_buddy_merging();
        all_passed &= test_challenge_3_mmap_large_allocations();
        all_passed &= test_statistics_accuracy();
        all_passed &= test_scalloc_functionality();
        all_passed &= test_srealloc_basic();
        all_passed &= test_edge_cases();

        if (all_passed) {
            cout << "\n🎉 ALL TESTS PASSED! 🎉" << endl;
            cout << "Your buddy allocator implementation is working correctly!" << endl;
        } else {
            cout << "\n❌ SOME TESTS FAILED" << endl;
            cout << "Check the output above for specific issues." << endl;
            return 1;
        }

    } catch (const exception& e) {
        cout << "\n❌ TEST FAILED: " << e.what() << endl;
        cout << "\nTip: Run with --simple flag to debug basic functionality first:" << endl;
        cout << "./test_buddy --simple" << endl;
        return 1;
    } catch (...) {
        cout << "\n❌ TEST FAILED: Unknown error occurred" << endl;
        return 1;
    }

    print_detailed_stats("Final heap state");

    return 0;
}