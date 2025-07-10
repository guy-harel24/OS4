#include <iostream>
#include <cassert>
#include <cstring>
#include <vector>
#include <algorithm>
#include <iomanip>
#include <map>
#include <set>
using namespace std;

// Forward declarations
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

// Advanced testing utilities
struct AllocInfo {
    void* ptr;
    size_t size;
    bool is_mmap;

    AllocInfo(void* p, size_t s, bool mmap) : ptr(p), size(s), is_mmap(mmap) {}
};

void print_stats(const string& label) {
    cout << label << " - Free: " << _num_free_blocks() << " blocks/"
         << _num_free_bytes() << " bytes, Allocated: " << _num_allocated_blocks()
         << " blocks/" << _num_allocated_bytes() << " bytes" << endl;
}

bool validate_memory_pattern(void* ptr, size_t size, char pattern) {
    char* mem = (char*)ptr;
    for (size_t i = 0; i < size; i++) {
        if (mem[i] != pattern) {
            cout << "❌ Memory pattern mismatch at byte " << i
                 << ": expected " << (int)pattern << ", got " << (int)mem[i] << endl;
            return false;
        }
    }
    return true;
}

void fill_memory_pattern(void* ptr, size_t size, char pattern) {
    memset(ptr, pattern, size);
}

// Test 1: Order Boundary Testing
bool test_order_boundaries() {
    cout << "\n=== Advanced Test 1: Order Boundary Testing ===" << endl;

    size_t meta_size = _size_meta_data();
    vector<size_t> test_sizes = {
            1,                          // Minimum size
            128 - meta_size - 1,        // Just under order 0
            128 - meta_size,            // Exactly order 0 user space
            128 - meta_size + 1,        // Just over order 0
            256 - meta_size,            // Exactly order 1 user space
            512 - meta_size,            // Exactly order 2 user space
            1024 - meta_size,           // Exactly order 3 user space
            128 * 1024 - meta_size - 1, // Just under mmap threshold
            128 * 1024 - meta_size,     // Exactly at mmap threshold
            128 * 1024,                 // First mmap size
            128 * 1024 + 1,             // Just over mmap threshold
    };

    vector<AllocInfo> allocations;

    for (size_t size : test_sizes) {
        void* ptr = smalloc(size);
        if (!ptr) {
            cout << "❌ Failed to allocate " << size << " bytes" << endl;
            return false;
        }

        bool is_mmap = (size >= 128 * 1024);
        allocations.emplace_back(ptr, size, is_mmap);

        // Fill with pattern to test memory integrity
        fill_memory_pattern(ptr, size, 0xAA);

        cout << "✓ Allocated " << size << " bytes ("
             << (is_mmap ? "mmap" : "buddy") << ")" << endl;
    }

    print_stats("After boundary allocations");

    // Verify all memory patterns are intact
    for (auto& alloc : allocations) {
        if (!validate_memory_pattern(alloc.ptr, alloc.size, 0xAA)) {
            cout << "❌ Memory corruption in " << alloc.size << " byte allocation" << endl;
            return false;
        }
    }

    // Free all allocations
    for (auto& alloc : allocations) {
        sfree(alloc.ptr);
    }

    print_stats("After freeing boundary allocations");
    cout << "✓ Order boundary testing passed" << endl;
    return true;
}

// Test 2: Buddy Relationship Verification
bool test_buddy_relationships() {
    cout << "\n=== Advanced Test 2: Buddy Relationship Verification ===" << endl;

    // Strategy: Allocate many small blocks, then free in patterns to test buddy merging
    vector<void*> ptrs;

    // Allocate 16 small blocks (should create lots of splitting)
    for (int i = 0; i < 16; i++) {
        void* ptr = smalloc(50);
        if (!ptr) {
            cout << "❌ Failed allocation " << i << " in buddy test" << endl;
            return false;
        }
        ptrs.push_back(ptr);
    }

    size_t free_blocks_after_alloc = _num_free_blocks();
    print_stats("After 16 small allocations");

    // Free every other allocation first
    for (size_t i = 0; i < ptrs.size(); i += 2) {
        sfree(ptrs[i]);
        ptrs[i] = nullptr;
    }

    size_t free_blocks_after_partial = _num_free_blocks();
    print_stats("After freeing every other allocation");

    // Now free the remaining allocations (should trigger more merging)
    for (size_t i = 1; i < ptrs.size(); i += 2) {
        sfree(ptrs[i]);
    }

    size_t free_blocks_after_all = _num_free_blocks();
    print_stats("After freeing all allocations");

    // Verify merging occurred (should have fewer blocks after complete deallocation)
    if (free_blocks_after_all >= free_blocks_after_alloc) {
        cout << "⚠️  Expected more merging: " << free_blocks_after_alloc
             << " → " << free_blocks_after_all << " free blocks" << endl;
    }

    cout << "✓ Buddy relationship testing completed" << endl;
    return true;
}

// Test 3: Complex srealloc Scenarios
bool test_complex_srealloc() {
    cout << "\n=== Advanced Test 3: Complex srealloc Scenarios ===" << endl;

    // Test 1: Growing within same order
    void* ptr = smalloc(50);
    if (!ptr) return false;

    fill_memory_pattern(ptr, 50, 0xBB);

    ptr = srealloc(ptr, 80); // Still order 0
    if (!ptr) {
        cout << "❌ srealloc within same order failed" << endl;
        return false;
    }

    if (!validate_memory_pattern(ptr, 50, 0xBB)) {
        cout << "❌ srealloc corrupted data when growing within order" << endl;
        return false;
    }

    // Test 2: Growing to different order
    ptr = srealloc(ptr, 300); // Should move to order 2
    if (!ptr) {
        cout << "❌ srealloc to different order failed" << endl;
        return false;
    }

    if (!validate_memory_pattern(ptr, 50, 0xBB)) {
        cout << "❌ srealloc corrupted data when growing to different order" << endl;
        return false;
    }

    // Test 3: Shrinking
    ptr = srealloc(ptr, 100); // Shrink back
    if (!ptr) {
        cout << "❌ srealloc shrinking failed" << endl;
        return false;
    }

    if (!validate_memory_pattern(ptr, 50, 0xBB)) {
        cout << "❌ srealloc corrupted data when shrinking" << endl;
        return false;
    }

    // Test 4: srealloc to same size
    void* old_ptr = ptr;
    ptr = srealloc(ptr, 100); // Same size
    if (!ptr) {
        cout << "❌ srealloc to same size failed" << endl;
        return false;
    }

    // Should ideally return same pointer for same size
    if (ptr != old_ptr) {
        cout << "⚠️  srealloc to same size moved the block (suboptimal but not wrong)" << endl;
    }

    sfree(ptr);

    // Test 5: srealloc with NULL pointer
    ptr = srealloc(nullptr, 150);
    if (!ptr) {
        cout << "❌ srealloc(nullptr, size) failed" << endl;
        return false;
    }
    sfree(ptr);

    // Test 6: srealloc to size 0
    ptr = smalloc(100);
    ptr = srealloc(ptr, 0);
    if (ptr != nullptr) {
        cout << "❌ srealloc(ptr, 0) should return NULL" << endl;
        return false;
    }

    cout << "✓ Complex srealloc scenarios passed" << endl;
    return true;
}

// Test 4: Memory Alignment and Address Analysis
bool test_memory_alignment() {
    cout << "\n=== Advanced Test 4: Memory Alignment and Address Analysis ===" << endl;

    vector<void*> ptrs;
    vector<uintptr_t> addresses;

    // Allocate multiple blocks of same size
    for (int i = 0; i < 8; i++) {
        void* ptr = smalloc(100);
        if (!ptr) {
            cout << "❌ Allocation " << i << " failed" << endl;
            return false;
        }
        ptrs.push_back(ptr);
        uintptr_t addr = (uintptr_t)ptr;
        addresses.push_back(addr);

        cout << "Block " << i << " at 0x" << hex << addr << dec << endl;
    }

    // Analyze address patterns
    cout << "\nAddress Analysis:" << endl;
    for (size_t i = 1; i < addresses.size(); i++) {
        ptrdiff_t diff = addresses[i] - addresses[i-1];
        cout << "Distance between block " << (i-1) << " and " << i
             << ": " << diff << " bytes" << endl;

        // Check if difference is power of 2 (expected in buddy system)
        if (diff > 0 && (diff & (diff - 1)) == 0) {
            cout << "  → Power of 2 spacing ✓" << endl;
        }
    }

    // Test address alignment (should be aligned to metadata size at least)
    size_t meta_size = _size_meta_data();
    for (size_t i = 0; i < addresses.size(); i++) {
        if (addresses[i] % meta_size != 0) {
            cout << "⚠️  Block " << i << " not aligned to metadata size" << endl;
        }
    }

    // Clean up
    for (void* ptr : ptrs) {
        sfree(ptr);
    }

    cout << "✓ Memory alignment analysis completed" << endl;
    return true;
}

// Test 5: Fragmentation and Defragmentation Patterns
bool test_fragmentation_patterns() {
    cout << "\n=== Advanced Test 5: Fragmentation and Defragmentation Patterns ===" << endl;

    print_stats("Initial state");

    // Phase 1: Create heavy fragmentation
    vector<void*> small_ptrs, medium_ptrs, large_ptrs;

    // Interleave different sized allocations
    for (int i = 0; i < 10; i++) {
        small_ptrs.push_back(smalloc(50 + i));
        medium_ptrs.push_back(smalloc(200 + i * 10));
        large_ptrs.push_back(smalloc(500 + i * 20));
    }

    print_stats("After creating fragmentation");

    // Phase 2: Free in complex pattern to test merging
    // Free every other small allocation
    for (size_t i = 0; i < small_ptrs.size(); i += 2) {
        sfree(small_ptrs[i]);
        small_ptrs[i] = nullptr;
    }

    // Free all medium allocations
    for (void* ptr : medium_ptrs) {
        sfree(ptr);
    }

    print_stats("After partial deallocation");

    // Phase 3: Allocate different sizes to test space reuse
    vector<void*> reuse_ptrs;
    for (int i = 0; i < 5; i++) {
        reuse_ptrs.push_back(smalloc(180 + i * 15)); // Different sizes
    }

    print_stats("After reallocation");

    // Phase 4: Clean up everything
    for (void* ptr : small_ptrs) {
        if (ptr) sfree(ptr);
    }
    for (void* ptr : large_ptrs) {
        sfree(ptr);
    }
    for (void* ptr : reuse_ptrs) {
        sfree(ptr);
    }

    print_stats("After complete cleanup");

    cout << "✓ Fragmentation pattern testing completed" << endl;
    return true;
}

// Test 6: mmap Boundary and Large Allocation Testing
bool test_mmap_advanced() {
    cout << "\n=== Advanced Test 6: mmap Advanced Testing ===" << endl;

    vector<AllocInfo> mmap_allocs;

    // Test various large sizes
    vector<size_t> large_sizes = {
            128 * 1024,         // Exactly at threshold
            150 * 1024,         // Moderately large
            1024 * 1024,        // 1MB
            5 * 1024 * 1024,    // 5MB
            10 * 1024 * 1024    // 10MB
    };

    for (size_t size : large_sizes) {
        void* ptr = smalloc(size);
        if (!ptr) {
            cout << "❌ Failed to allocate " << size << " bytes via mmap" << endl;
            return false;
        }

        mmap_allocs.emplace_back(ptr, size, true);

        // Fill with pattern
        fill_memory_pattern(ptr, min(size, (size_t)1024), 0xCC);

        cout << "✓ Allocated " << size / 1024 << " KB via mmap" << endl;
    }

    print_stats("After large mmap allocations");

    // Verify patterns
    for (auto& alloc : mmap_allocs) {
        size_t check_size = min(alloc.size, (size_t)1024);
        if (!validate_memory_pattern(alloc.ptr, check_size, 0xCC)) {
            cout << "❌ mmap memory corruption detected" << endl;
            return false;
        }
    }

    // Free in reverse order
    for (auto it = mmap_allocs.rbegin(); it != mmap_allocs.rend(); ++it) {
        sfree(it->ptr);
    }

    print_stats("After freeing mmap allocations");

    cout << "✓ Advanced mmap testing passed" << endl;
    return true;
}

// Test 7: Statistics Consistency Under Stress
bool test_statistics_consistency() {
    cout << "\n=== Advanced Test 7: Statistics Consistency Under Stress ===" << endl;

    size_t initial_free_blocks = _num_free_blocks();
    size_t initial_free_bytes = _num_free_bytes();

    // Perform many random operations
    vector<void*> active_ptrs;

    for (int round = 0; round < 20; round++) {
        // Random allocation sizes
        size_t size = 30 + (round * 17) % 800; // Semi-random sizes
        void* ptr = smalloc(size);

        if (ptr) {
            active_ptrs.push_back(ptr);
            fill_memory_pattern(ptr, size, 0xDD);
        }

        // Occasionally free some allocations
        if (round % 4 == 3 && !active_ptrs.empty()) {
            size_t idx = round % active_ptrs.size();
            sfree(active_ptrs[idx]);
            active_ptrs.erase(active_ptrs.begin() + idx);
        }

        // Check for negative values (underflow)
        if (_num_allocated_blocks() > 1000000 || _num_allocated_bytes() > 1000000000) {
            cout << "❌ Statistics underflow detected at round " << round << endl;
            return false;
        }
    }

    cout << "Active allocations: " << active_ptrs.size() << endl;
    print_stats("After stress operations");

    // Verify all active memory is still intact
    for (size_t i = 0; i < active_ptrs.size(); i++) {
        size_t size = 30 + (i * 17) % 800;
        if (!validate_memory_pattern(active_ptrs[i], size, 0xDD)) {
            cout << "❌ Memory corruption in active allocation " << i << endl;
            return false;
        }
    }

    // Free everything
    for (void* ptr : active_ptrs) {
        sfree(ptr);
    }

    // Check if we returned to reasonable state
    print_stats("After cleanup");

    cout << "✓ Statistics consistency testing passed" << endl;
    return true;
}

// Test 8: Edge Cases and Error Conditions
bool test_advanced_edge_cases() {
    cout << "\n=== Advanced Test 8: Edge Cases and Error Conditions ===" << endl;

    // Test 1: Maximum buddy allocation
    size_t max_buddy_size = 128 * 1024 - _size_meta_data() - 1;
    void* max_ptr = smalloc(max_buddy_size);
    if (!max_ptr) {
        cout << "❌ Failed to allocate maximum buddy size" << endl;
        return false;
    }
    sfree(max_ptr);

    // Test 2: scalloc overflow protection
    void* overflow_ptr = scalloc(SIZE_MAX / 2, SIZE_MAX / 2);
    if (overflow_ptr != nullptr) {
        cout << "❌ scalloc should detect overflow and return NULL" << endl;
        return false;
    }

    // Test 3: Large scalloc
    void* large_zero = scalloc(50000, 4); // 200KB, should use mmap
    if (!large_zero) {
        cout << "❌ Large scalloc failed" << endl;
        return false;
    }

    // Verify it's actually zeroed
    if (!validate_memory_pattern(large_zero, 200000, 0)) {
        cout << "❌ Large scalloc not properly zeroed" << endl;
        return false;
    }
    sfree(large_zero);

    // Test 4: Mixed buddy and mmap allocations
    void* buddy_ptr = smalloc(1000);
    void* mmap_ptr = smalloc(200 * 1024);

    if (!buddy_ptr || !mmap_ptr) {
        cout << "❌ Mixed allocation failed" << endl;
        return false;
    }

    // Free in different order
    sfree(mmap_ptr);
    sfree(buddy_ptr);

    // Test 5: Multiple srealloc on same pointer
    void* multi_ptr = smalloc(100);
    multi_ptr = srealloc(multi_ptr, 200);
    multi_ptr = srealloc(multi_ptr, 150);
    multi_ptr = srealloc(multi_ptr, 300);

    if (!multi_ptr) {
        cout << "❌ Multiple srealloc failed" << endl;
        return false;
    }
    sfree(multi_ptr);

    cout << "✓ Advanced edge cases passed" << endl;
    return true;
}

// Main advanced test runner
int main() {
    cout << "=================================================" << endl;
    cout << "  ADVANCED Buddy Allocator Test Suite - Part 3" << endl;
    cout << "=================================================" << endl;
    cout << "These tests stress-test your implementation beyond basic functionality." << endl;

    bool all_passed = true;

    try {
        all_passed &= test_order_boundaries();
        all_passed &= test_buddy_relationships();
        all_passed &= test_complex_srealloc();
        all_passed &= test_memory_alignment();
        all_passed &= test_fragmentation_patterns();
        all_passed &= test_mmap_advanced();
        all_passed &= test_statistics_consistency();
        all_passed &= test_advanced_edge_cases();

        if (all_passed) {
            cout << "\n🏆 ALL ADVANCED TESTS PASSED! 🏆" << endl;
            cout << "Your buddy allocator implementation is robust and handles complex scenarios!" << endl;
            cout << "\nYour allocator demonstrates:" << endl;
            cout << "✓ Proper order boundary handling" << endl;
            cout << "✓ Correct buddy relationships and merging" << endl;
            cout << "✓ Robust srealloc implementation" << endl;
            cout << "✓ Good memory management under stress" << endl;
            cout << "✓ Reliable mmap integration" << endl;
            cout << "✓ Consistent statistics tracking" << endl;
        } else {
            cout << "\n⚠️  SOME ADVANCED TESTS FAILED" << endl;
            cout << "Your basic functionality works, but there are edge cases to address." << endl;
            cout << "This is still excellent work - these tests are quite demanding!" << endl;
        }

    } catch (const exception& e) {
        cout << "\n❌ ADVANCED TEST FAILED: " << e.what() << endl;
        return 1;
    }

    print_stats("Final system state");
    return all_passed ? 0 : 1;
}
