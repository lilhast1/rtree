#define CATCH_CONFIG_MAIN
#include <algorithm>
#include <catch2/catch_all.hpp>
#include <random>
#include <set>

#include "rtree/rtree.h"  // Adjust include path for your Gutman::RTree
#include "rtree_hilbert/hilbert.h"

// Helper alias (assuming you have makeRect defined somewhere)
// using Rectangle = Gutman::RTree<int>::Rectangle;

// Helper function to create a rectangle
using Rectangle = Gutman::Rectangle;

// Helper function to create a rectangle
Rectangle makeRect(std::vector<double> min, std::vector<double> max) {
    return Rectangle(min, max);
}

// using Rectangle = decltype(makeRect(std::vector<double>{}, std::vector<double>{}));
template <typename T>
void collect_all_elements(Gutman::Node<T>* node, std::vector<T*>& elements) {
    if (!node)
        return;

    if (node->is_leaf) {
        for (auto& elem : node->elems) {
            elements.push_back(elem.first);
        }
    } else {
        for (auto child : node->children) {
            collect_all_elements(child, elements);
        }
    }
}

// Helper to count nodes at each level
template <typename T>
void count_tree_structure(Gutman::Node<T>* node, int level, std::map<int, int>& level_counts) {
    if (!node)
        return;
    level_counts[level]++;

    if (!node->is_leaf) {
        for (auto child : node->children) {
            count_tree_structure(child, level + 1, level_counts);
        }
    }
}
// ================= MBR Correctness Tests =================
TEST_CASE("MBR correctness after operations", "[mbr]") {
    Gutman::RTree<int> tree(2, 4);
    std::vector<int> values(50);
    std::vector<Rectangle> rects;

    // Insert scattered rectangles
    for (int i = 0; i < 50; i++) {
        values[i] = i;
        double x = (i % 10) * 3.0;
        double y = (i / 10) * 3.0;
        rects.push_back(makeRect({x, y}, {x + 1.5, y + 1.5}));
        tree.insert(rects[i], &values[i]);
    }

    SECTION("All inserted elements are findable") {
        for (int i = 0; i < 50; i++) {
            auto results = tree.search(rects[i]);
            bool found = false;
            for (auto* ptr : results) {
                if (*ptr == i)
                    found = true;
            }
            REQUIRE(found);
        }
    }

    SECTION("After deletions, MBRs still cover remaining elements") {
        // Delete every third element
        for (int i = 0; i < 50; i += 3) {
            tree.remove(rects[i]);
        }

        // Check that remaining elements are still findable
        for (int i = 0; i < 50; i++) {
            if (i % 3 == 0)
                continue;  // Skip deleted

            auto results = tree.search(rects[i]);
            bool found = false;
            for (auto* ptr : results) {
                if (*ptr == i)
                    found = true;
            }
            REQUIRE(found);
        }
    }
}

// ================= Randomized Operations Tests =================
TEST_CASE("Randomized insert/delete operations", "[random]") {
    std::mt19937 rng(12345);  // Fixed seed for reproducibility
    std::uniform_real_distribution<double> coord_dist(0.0, 100.0);
    std::uniform_real_distribution<double> size_dist(0.5, 5.0);

    Gutman::RTree<int> tree(3, 6);
    // FIX: Pre-allocate to prevent reallocation
    std::vector<int> values(200);
    std::vector<Rectangle> rects;
    std::set<int> active_indices;

    SECTION("Random insertions and deletions maintain correctness") {
        // Insert 200 random rectangles
        for (int i = 0; i < 200; i++) {
            values[i] = i;  // FIX: Use index instead of push_back
            double x = coord_dist(rng);
            double y = coord_dist(rng);
            double w = size_dist(rng);
            double h = size_dist(rng);
            rects.push_back(makeRect({x, y}, {x + w, y + h}));
            tree.insert(rects[i], &values[i]);
            active_indices.insert(i);
        }

        // Randomly delete 100 elements
        std::vector<int> to_delete;
        for (int i = 0; i < 200; i++) {
            if (i % 2 == 0)
                to_delete.push_back(i);
        }
        std::shuffle(to_delete.begin(), to_delete.end(), rng);

        for (int idx : to_delete) {
            tree.remove(rects[idx]);
            active_indices.erase(idx);
        }

        // Verify all remaining elements are findable
        for (int idx : active_indices) {
            auto results = tree.search(rects[idx]);
            bool found = false;
            for (auto* ptr : results) {
                if (*ptr == idx)
                    found = true;
            }
            REQUIRE(found);
        }

        // Verify deleted elements are not found
        for (int idx : to_delete) {
            auto results = tree.search(rects[idx]);
            bool found = false;
            for (auto* ptr : results) {
                if (*ptr == idx)
                    found = true;
            }
            REQUIRE_FALSE(found);
        }
    }
}

// ================= Boundary and Overlap Tests =================
TEST_CASE("Boundary touch and overlap search", "[boundary]") {
    Gutman::RTree<int> tree(2, 4);
    std::vector<int> values(9);
    std::vector<Rectangle> rects;

    // Create a 3x3 grid of touching rectangles
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            int idx = i * 3 + j;
            values[idx] = idx;
            rects.push_back(makeRect({(double)j, (double)i}, {(double)(j + 1), (double)(i + 1)}));
            tree.insert(rects[idx], &values[idx]);
        }
    }

    SECTION("Point on boundary finds adjacent rectangles") {
        // Point at (1, 1) should find rectangles 0, 1, 3, 4
        auto results = tree.search(makeRect({1.0, 1.0}, {1.0, 1.0}));
        std::set<int> found;
        for (auto* ptr : results) {
            found.insert(*ptr);
        }
        REQUIRE(found.count(0) == 1);
        REQUIRE(found.count(1) == 1);
        REQUIRE(found.count(3) == 1);
        REQUIRE(found.count(4) == 1);
    }

    SECTION("Search spanning multiple cells") {
        // Rectangle from (0.5, 0.5) to (2.5, 2.5) should find all 9
        auto results = tree.search(makeRect({0.5, 0.5}, {2.5, 2.5}));
        REQUIRE(results.size() == 9);
    }
}

// ================= Stress Test with Many Small Operations =================
TEST_CASE("Many small delete-reinsert cycles", "[stress-cycles]") {
    Gutman::RTree<int> tree(2, 4);
    std::vector<int> values(100);
    std::vector<Rectangle> rects;

    for (int i = 0; i < 100; i++) {
        values[i] = i;
        double x = (i % 10) * 2.0;
        double y = (i / 10) * 2.0;
        rects.push_back(makeRect({x, y}, {x + 1.0, y + 1.0}));
        tree.insert(rects[i], &values[i]);
    }

    // Perform 50 cycles of delete-reinsert
    for (int cycle = 0; cycle < 50; cycle++) {
        // Delete elements at positions cycle, cycle+10, cycle+20, ...
        std::vector<int> to_delete;
        for (int i = cycle % 10; i < 100; i += 10) {
            to_delete.push_back(i);
        }

        for (int idx : to_delete) {
            tree.remove(rects[idx]);
        }

        // Reinsert them
        for (int idx : to_delete) {
            tree.insert(rects[idx], &values[idx]);
        }
    }

    // Final verification - all elements should be present
    auto results = tree.search(makeRect({-10, -10}, {30, 30}));
    REQUIRE(results.size() == 100);

    std::set<int> found;
    for (auto* ptr : results) {
        found.insert(*ptr);
    }
    REQUIRE(found.size() == 100);
}

// ================= Empty Tree Edge Cases =================
TEST_CASE("Empty tree operations", "[empty]") {
    Gutman::RTree<int> tree(2, 4);

    SECTION("Search on empty tree returns nothing") {
        auto results = tree.search(makeRect({0, 0}, {10, 10}));
        REQUIRE(results.size() == 0);
    }

    SECTION("Delete from empty tree doesn't crash") {
        REQUIRE_NOTHROW(tree.remove(makeRect({0, 0}, {1, 1})));
    }

    SECTION("Insert then delete all elements") {
        std::vector<int> values(5);
        std::vector<Rectangle> rects;

        for (int i = 0; i < 5; i++) {
            values[i] = i;
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }

        for (int i = 0; i < 5; i++) {
            tree.remove(rects[i]);
        }

        auto results = tree.search(makeRect({-10, -10}, {20, 20}));
        REQUIRE(results.size() == 0);
    }
}

// ================= Duplicate Rectangle Handling =================
TEST_CASE("Multiple elements with same rectangle", "[duplicates]") {
    Gutman::RTree<int> tree(2, 4);
    std::vector<int> values = {10, 20, 30, 40, 50};
    auto rect = makeRect({5.0, 5.0}, {10.0, 10.0});

    SECTION("Insert multiple elements with identical rectangles") {
        for (auto& v : values) {
            tree.insert(rect, &v);
        }

        auto results = tree.search(rect);
        REQUIRE(results.size() == 5);

        std::set<int> found;
        for (auto* ptr : results) {
            found.insert(*ptr);
        }
        REQUIRE(found.size() == 5);
    }

    SECTION("Delete some duplicates") {
        for (auto& v : values) {
            tree.insert(rect, &v);
        }

        // Delete first 3
        for (int i = 0; i < 3; i++) {
            tree.remove(rect);
        }

        auto results = tree.search(rect);
        REQUIRE(results.size() == 2);
    }
}

// ================= Non-overlapping Regions =================
TEST_CASE("Non-overlapping spatial regions", "[regions]") {
    Gutman::RTree<int> tree(2, 4);
    std::vector<int> values(20);
    std::vector<Rectangle> rects;

    // Create 4 clusters in different quadrants
    for (int i = 0; i < 20; i++) {
        values[i] = i;
        int quadrant = i / 5;
        int within = i % 5;

        double base_x = (quadrant % 2) * 50.0;
        double base_y = (quadrant / 2) * 50.0;
        double x = base_x + within;
        double y = base_y + within;

        rects.push_back(makeRect({x, y}, {x + 0.5, y + 0.5}));
        tree.insert(rects[i], &values[i]);
    }

    SECTION("Search in one quadrant finds only those elements") {
        auto q1_results = tree.search(makeRect({0, 0}, {10, 10}));
        REQUIRE(q1_results.size() == 5);

        auto q2_results = tree.search(makeRect({50, 0}, {60, 10}));
        REQUIRE(q2_results.size() == 5);

        auto q3_results = tree.search(makeRect({0, 50}, {10, 60}));
        REQUIRE(q3_results.size() == 5);

        auto q4_results = tree.search(makeRect({50, 50}, {60, 60}));
        REQUIRE(q4_results.size() == 5);
    }

    SECTION("Delete one quadrant doesn't affect others") {
        // Delete quadrant 1
        for (int i = 0; i < 5; i++) {
            tree.remove(rects[i]);
        }

        auto q1_results = tree.search(makeRect({0, 0}, {10, 10}));
        REQUIRE(q1_results.size() == 0);

        auto q2_results = tree.search(makeRect({50, 0}, {60, 10}));
        REQUIRE(q2_results.size() == 5);
    }
}

// ================= Large Scale Deletion Test =================
TEST_CASE("Delete majority of elements", "[large-delete]") {
    Gutman::RTree<int> tree(3, 7);
    // FIX: Pre-allocate to prevent reallocation
    std::vector<int> values(1000);
    std::vector<Rectangle> rects;

    for (int i = 0; i < 1000; i++) {
        values[i] = i;
        double x = (i % 50) * 2.0;
        double y = (i / 50) * 2.0;
        rects.push_back(makeRect({x, y}, {x + 1.0, y + 1.0}));
        tree.insert(rects[i], &values[i]);
    }

    SECTION("Delete 90% of elements") {
        for (int i = 0; i < 900; i++) {
            tree.remove(rects[i]);
        }

        auto results = tree.search(makeRect({-10, -10}, {200, 200}));
        REQUIRE(results.size() == 100);

        // Verify correct elements remain
        std::set<int> found;
        for (auto* ptr : results) {
            found.insert(*ptr);
            REQUIRE(*ptr >= 900);
            REQUIRE(*ptr < 1000);
        }
        REQUIRE(found.size() == 100);
    }
}

// ================= Overlapping Rectangles Search =================
TEST_CASE("Search with heavily overlapping rectangles", "[overlap]") {
    Gutman::RTree<int> tree(2, 4);
    // FIX: Pre-allocate to prevent reallocation
    std::vector<int> values(30);
    std::vector<Rectangle> rects;

    // Create overlapping rectangles that ALL contain point (5,5)
    for (int i = 0; i < 30; i++) {
        values[i] = i;
        // Each rectangle expands in all directions from center (5,5)
        double expansion = 1.0 + i * 0.3;
        rects.push_back(
            makeRect({5.0 - expansion, 5.0 - expansion}, {5.0 + expansion, 5.0 + expansion}));
        tree.insert(rects[i], &values[i]);
    }

    SECTION("Point search in center finds many rectangles") {
        auto results = tree.search(makeRect({5.0, 5.0}, {5.0, 5.0}));
        // All 30 should contain point (5,5) - they're all centered on it
        REQUIRE(results.size() == 30);
    }

    SECTION("After deleting some, search still correct") {
        for (int i = 0; i < 15; i++) {
            tree.remove(rects[i]);
        }

        auto results = tree.search(makeRect({5.0, 5.0}, {5.0, 5.0}));

        std::set<int> found;
        for (auto* ptr : results) {
            found.insert(*ptr);
            REQUIRE(*ptr >= 15);  // Only elements 15-29 should remain
        }

        REQUIRE(found.size() == 15);
    }
}

// ================= Tree Structure Integrity Test =================
TEST_CASE("Tree maintains valid structure", "[structure]") {
    Gutman::RTree<int> tree(2, 4);
    std::vector<int> values(100);
    std::vector<Rectangle> rects;

    for (int i = 0; i < 100; i++) {
        values[i] = i;
        double x = (i % 10) * 3.0;
        double y = (i / 10) * 3.0;
        rects.push_back(makeRect({x, y}, {x + 1.5, y + 1.5}));
        tree.insert(rects[i], &values[i]);
    }

    SECTION("After mixed operations, no element is lost") {
        std::set<int> deleted;

        // Delete elements 10-30
        for (int i = 10; i < 30; i++) {
            tree.remove(rects[i]);
            deleted.insert(i);
        }

        // Search for all
        auto results = tree.search(makeRect({-10, -10}, {50, 50}));

        std::set<int> found;
        for (auto* ptr : results) {
            found.insert(*ptr);
        }

        REQUIRE(found.size() == 80);  // 100 - 20 deleted

        // Verify no deleted elements are found
        for (int i : deleted) {
            REQUIRE(found.count(i) == 0);
        }

        // Verify all non-deleted elements are found
        for (int i = 0; i < 100; i++) {
            if (deleted.count(i) == 0) {
                REQUIRE(found.count(i) == 1);
            }
        }
    }
}
TEST_CASE("Debug delete with detailed tracking", "[debug]") {
    const int MIN_CHILD = 2;
    const int MAX_CHILD = 4;

    Gutman::RTree<int> tree(MIN_CHILD, MAX_CHILD);

    // Create a simple test case
    std::vector<int> values(20);
    std::vector<Rectangle> rects;

    std::cout << "\n=== INSERTION PHASE ===" << std::endl;
    for (int i = 0; i < 20; i++) {
        values[i] = i;
        double x = (i % 5) * 2.0;
        double y = (i / 5) * 2.0;
        rects.push_back(makeRect({x, y}, {x + 1.0, y + 1.0}));

        std::cout << "Inserting element " << i << " at (" << x << "," << y << ")" << std::endl;
        tree.insert(rects[i], &values[i]);
    }

    // Verify initial state
    std::cout << "\n=== INITIAL STATE ===" << std::endl;
    auto initial_results = tree.search(makeRect({-10, -10}, {20, 20}));
    std::cout << "Total elements after insertion: " << initial_results.size() << std::endl;

    std::set<int> initial_set;
    for (auto* ptr : initial_results) {
        initial_set.insert(*ptr);
    }
    std::cout << "Unique elements: " << initial_set.size() << std::endl;

    if (initial_set.size() != 20) {
        std::cout << "ERROR: Already have duplicates after insertion!" << std::endl;
        std::cout << "Elements found: ";
        for (int val : initial_set) {
            std::cout << val << " ";
        }
        std::cout << std::endl;
    }

    // Now delete some elements
    std::vector<int> to_delete = {0, 1, 2, 5, 6, 7, 10, 11, 12, 15, 16, 17};

    std::cout << "\n=== DELETION PHASE ===" << std::endl;
    for (int idx : to_delete) {
        std::cout << "\n--- Deleting element " << idx << " ---" << std::endl;

        // Check before deletion
        auto before_results = tree.search(makeRect({-10, -10}, {20, 20}));
        std::set<int> before_set;
        for (auto* ptr : before_results) {
            before_set.insert(*ptr);
        }
        std::cout << "Before delete: " << before_results.size() << " total, " << before_set.size()
                  << " unique" << std::endl;

        // Perform deletion
        tree.remove(rects[idx]);

        // Check after deletion
        auto after_results = tree.search(makeRect({-10, -10}, {20, 20}));
        std::set<int> after_set;
        for (auto* ptr : after_results) {
            after_set.insert(*ptr);
        }
        std::cout << "After delete: " << after_results.size() << " total, " << after_set.size()
                  << " unique" << std::endl;

        // Check if we have duplicates
        if (after_results.size() != after_set.size()) {
            std::cout << "!!! DUPLICATES DETECTED !!!" << std::endl;

            // Find which elements are duplicated
            std::map<int, int> counts;
            for (auto* ptr : after_results) {
                counts[*ptr]++;
            }

            std::cout << "Duplicate elements: ";
            for (auto& [val, count] : counts) {
                if (count > 1) {
                    std::cout << val << "(x" << count << ") ";
                }
            }
            std::cout << std::endl;
        }

        // Verify the deleted element is gone
        if (after_set.find(idx) != after_set.end()) {
            std::cout << "ERROR: Element " << idx << " still in tree after deletion!" << std::endl;
        }

        // Check expected count
        int expected = before_set.size() - 1;
        if (before_set.find(idx) == before_set.end()) {
            expected = before_set.size();  // Element wasn't there
        }

        if (after_set.size() != expected) {
            std::cout << "ERROR: Expected " << expected << " unique elements, got "
                      << after_set.size() << std::endl;
        }
    }

    // Final verification
    std::cout << "\n=== FINAL STATE ===" << std::endl;
    auto final_results = tree.search(makeRect({-10, -10}, {20, 20}));
    std::set<int> final_set;
    for (auto* ptr : final_results) {
        final_set.insert(*ptr);
    }

    std::cout << "Final count: " << final_results.size() << " total, " << final_set.size()
              << " unique" << std::endl;
    std::cout << "Expected: 8 elements (20 - 12 deleted)" << std::endl;

    std::cout << "Remaining elements: ";
    for (int val : final_set) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    std::cout << "Expected elements: 3 4 8 9 13 14 18 19" << std::endl;

    REQUIRE(final_results.size() == 8);
    REQUIRE(final_set.size() == 8);
}

TEST_CASE("Minimal reproduction of duplicate bug", "[debug-minimal]") {
    const int MIN_CHILD = 2;
    const int MAX_CHILD = 4;

    Gutman::RTree<int> tree(MIN_CHILD, MAX_CHILD);

    // Insert just enough to cause a split and then test deletion
    std::vector<int> values(10);
    std::vector<Rectangle> rects;

    std::cout << "\n=== MINIMAL TEST ===" << std::endl;

    for (int i = 0; i < 10; i++) {
        values[i] = i + 100;  // Use 100+ to distinguish from indices
        double x = i * 1.0;
        double y = i * 1.0;
        rects.push_back(makeRect({x, y}, {x + 0.5, y + 0.5}));
        tree.insert(rects[i], &values[i]);
    }

    std::cout << "Inserted 10 elements (100-109)" << std::endl;

    auto before = tree.search(makeRect({-10, -10}, {20, 20}));
    std::cout << "Before deletion: " << before.size() << " elements" << std::endl;

    // Delete first 3 elements
    for (int i = 0; i < 3; i++) {
        std::cout << "\nDeleting element " << (100 + i) << std::endl;
        tree.remove(rects[i]);

        auto current = tree.search(makeRect({-10, -10}, {20, 20}));
        std::set<int> unique;
        for (auto* p : current) {
            unique.insert(*p);
        }

        std::cout << "After deletion: " << current.size() << " total, " << unique.size()
                  << " unique" << std::endl;

        if (current.size() != unique.size()) {
            std::cout << "DUPLICATE FOUND after deleting " << (100 + i) << "!" << std::endl;

            // Show all elements
            std::map<int, int> counts;
            for (auto* p : current) {
                counts[*p]++;
            }
            for (auto& [val, cnt] : counts) {
                if (cnt > 1) {
                    std::cout << "  Value " << val << " appears " << cnt << " times" << std::endl;
                }
            }

            break;  // Stop at first duplicate
        }
    }

    auto final = tree.search(makeRect({-10, -10}, {20, 20}));
    std::set<int> final_unique;
    for (auto* p : final) {
        final_unique.insert(*p);
    }

    std::cout << "\nFinal: " << final.size() << " total, " << final_unique.size()
              << " unique (expected 7)" << std::endl;

    REQUIRE(final.size() == 7);
    REQUIRE(final_unique.size() == 7);
}
// ------------------- Insertion Tests -------------------
TEST_CASE("RTree insertion tests", "[insertion]") {
    SECTION("Insert single element") {
        Gutman::RTree<int> tree(2, 4);
        int val = 42;
        auto rect = makeRect({0.0, 0.0}, {1.0, 1.0});
        tree.insert(rect, &val);

        std::vector<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
        REQUIRE(*results[0] == 42);
    }

    SECTION("Insert multiple elements") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values = {1, 2, 3, 4, 5};
        std::vector<Rectangle> rects;
        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }

        std::vector<int*> results = tree.search(makeRect({0.0, 0.0}, {10.0, 10.0}));
        REQUIRE(results.size() == 5);
    }

    SECTION("Insert overlapping rectangles") {
        Gutman::RTree<int> tree(2, 4);
        int val1 = 10, val2 = 20, val3 = 30;
        auto rect1 = makeRect({0.0, 0.0}, {5.0, 5.0});
        auto rect2 = makeRect({3.0, 3.0}, {8.0, 8.0});
        auto rect3 = makeRect({4.0, 4.0}, {6.0, 6.0});
        tree.insert(rect1, &val1);
        tree.insert(rect2, &val2);
        tree.insert(rect3, &val3);

        std::vector<int*> results = tree.search(makeRect({4.0, 4.0}, {5.0, 5.0}));
        REQUIRE(results.size() == 3);
    }

    SECTION("Insert triggering node split") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values(10);
        for (int i = 0; i < 10; i++) {
            values[i] = i;
            auto rect = makeRect({(double)i, (double)i}, {(double)i + 0.5, (double)i + 0.5});
            tree.insert(rect, &values[i]);
        }
        std::vector<int*> results = tree.search(makeRect({-1.0, -1.0}, {20.0, 20.0}));
        REQUIRE(results.size() == 10);
    }

    SECTION("Insert identical rectangles with different values") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values = {1, 2, 3, 4, 5};
        auto rect = makeRect({5.0, 5.0}, {10.0, 10.0});
        for (auto& v : values) tree.insert(rect, &v);

        std::vector<int*> results = tree.search(rect);
        REQUIRE(results.size() == 5);
    }

    SECTION("Insert and search large dataset") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values(100);
        for (int i = 0; i < 100; i++) {
            values[i] = i;
            double x = (i % 10) * 2.0;
            double y = (i / 10) * 2.0;
            tree.insert(makeRect({x, y}, {x + 1.5, y + 1.5}), &values[i]);
        }
        std::vector<int*> results = tree.search(makeRect({-1.0, -1.0}, {30.0, 30.0}));
        REQUIRE(results.size() == 100);
    }
}

// ------------------- Search Tests -------------------
TEST_CASE("RTree search tests", "[search]") {
    SECTION("Search empty tree") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int*> results = tree.search(makeRect({0, 0}, {10, 10}));
        REQUIRE(results.size() == 0);
    }

    SECTION("Search no overlap") {
        Gutman::RTree<int> tree(2, 4);
        int val = 42;
        tree.insert(makeRect({0, 0}, {1, 1}), &val);
        std::vector<int*> results = tree.search(makeRect({10, 10}, {20, 20}));
        REQUIRE(results.size() == 0);
    }

    SECTION("Search partial overlap") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values = {1, 2, 3, 4, 5};
        tree.insert(makeRect({0, 0}, {2, 2}), &values[0]);
        tree.insert(makeRect({5, 5}, {7, 7}), &values[1]);
        tree.insert(makeRect({10, 10}, {12, 12}), &values[2]);
        tree.insert(makeRect({1, 1}, {3, 3}), &values[3]);
        tree.insert(makeRect({8, 8}, {9, 9}), &values[4]);

        std::vector<int*> results = tree.search(makeRect({0, 0}, {6, 6}));
        REQUIRE(results.size() == 3);
    }

    SECTION("Point query") {
        Gutman::RTree<int> tree(2, 4);
        int val = 99;
        tree.insert(makeRect({5, 5}, {10, 10}), &val);
        std::vector<int*> results = tree.search(makeRect({7, 7}, {7, 7}));
        REQUIRE(results.size() == 1);
        REQUIRE(*results[0] == 99);
    }

    SECTION("Search with exact boundaries") {
        Gutman::RTree<int> tree(2, 4);
        int val1 = 10, val2 = 20, val3 = 30;
        tree.insert(makeRect({0, 0}, {5, 5}), &val1);
        tree.insert(makeRect({5, 5}, {10, 10}), &val2);
        tree.insert(makeRect({10, 10}, {15, 15}), &val3);

        std::vector<int*> results = tree.search(makeRect({0, 0}, {5, 5}));
        REQUIRE(results.size() >= 1);
    }
}

// ------------------- Deletion Tests -------------------
TEST_CASE("RTree deletion tests", "[deletion]") {
    SECTION("Delete single element") {
        Gutman::RTree<int> tree(2, 4);
        int val = 42;
        auto rect = makeRect({0, 0}, {1, 1});
        tree.insert(rect, &val);
        tree.remove(rect);

        std::vector<int*> results = tree.search(rect);
        REQUIRE(results.size() == 0);
    }

    SECTION("Delete from multiple elements") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values = {1, 2, 3, 4, 5};
        std::vector<Rectangle> rects;
        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        tree.remove(rects[2]);
        std::vector<int*> results = tree.search(makeRect({0, 0}, {10, 10}));
        REQUIRE(results.size() == 4);
    }

    SECTION("Delete non-existent element") {
        Gutman::RTree<int> tree(2, 4);
        int val = 42;
        auto rect1 = makeRect({0, 0}, {1, 1});
        auto rect2 = makeRect({10, 10}, {11, 11});
        tree.insert(rect1, &val);
        tree.remove(rect2);

        std::vector<int*> results = tree.search(rect1);
        REQUIRE(results.size() == 1);
    }

    SECTION("Delete and reinsert") {
        Gutman::RTree<int> tree(2, 4);
        int val1 = 10, val2 = 20;
        auto rect = makeRect({0, 0}, {5, 5});
        tree.insert(rect, &val1);
        tree.remove(rect);
        tree.insert(rect, &val2);

        std::vector<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
        REQUIRE(*results[0] == 20);
    }

    SECTION("Delete multiple sequential elements") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values = {1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<Rectangle> rects;
        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        tree.remove(rects[1]);
        tree.remove(rects[3]);
        tree.remove(rects[5]);

        std::vector<int*> results = tree.search(makeRect({-1, -1}, {20, 20}));
        REQUIRE(results.size() == 5);
    }

    SECTION("Delete every other element") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values(20);
        std::vector<Rectangle> rects;
        for (int i = 0; i < 20; i++) {
            values[i] = i;
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 0.8, (double)i + 0.8}));
            tree.insert(rects[i], &values[i]);
        }
        for (int i = 0; i < 20; i += 2) tree.remove(rects[i]);

        std::vector<int*> results = tree.search(makeRect({-1, -1}, {25, 25}));
        REQUIRE(results.size() == 10);
    }

    SECTION("Delete from single element tree and reinsert") {
        Gutman::RTree<int> tree(2, 4);
        int val = 42;
        auto rect1 = makeRect({0, 0}, {1, 1});
        tree.insert(rect1, &val);
        tree.remove(rect1);

        int val2 = 99;
        auto rect2 = makeRect({5, 5}, {6, 6});
        tree.insert(rect2, &val2);

        std::vector<int*> results = tree.search(rect2);
        REQUIRE(results.size() == 1);
        REQUIRE(*results[0] == 99);
    }
}

// ------------------- Edge Cases -------------------
TEST_CASE("RTree edge cases", "[edge]") {
    SECTION("3D rectangles") {
        Gutman::RTree<int> tree(2, 4);
        int val = 42;
        auto rect = makeRect({0, 0, 0}, {1, 1, 1});
        tree.insert(rect, &val);
        std::vector<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
    }

    SECTION("High dimensional 5D") {
        Gutman::RTree<int> tree(2, 4);
        int val = 42;
        auto rect = makeRect({0, 0, 0, 0, 0}, {1, 1, 1, 1, 1});
        tree.insert(rect, &val);
        std::vector<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
    }

    SECTION("Zero area rectangle") {
        Gutman::RTree<int> tree(2, 4);
        int val = 42;
        auto rect = makeRect({5, 5}, {5, 5});
        tree.insert(rect, &val);
        std::vector<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
    }
}

// ------------------- Stress / Condense Tests -------------------
TEST_CASE("RTree stress and condense tests", "[stress]") {
    SECTION("Mixed insert/delete operations") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values(15);
        std::vector<Rectangle> rects;
        for (int i = 0; i < 5; i++) {
            values[i] = i;
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        tree.remove(rects[1]);
        tree.remove(rects[3]);
        for (int i = 5; i < 10; i++) {
            values[i] = i;
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        tree.remove(rects[2]);
        tree.remove(rects[6]);
        tree.remove(rects[8]);
        for (int i = 10; i < 15; i++) {
            values[i] = i;
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }

        std::vector<int*> results = tree.search(makeRect({-1, -1}, {20, 20}));
        REQUIRE(results.size() == 10);
    }

    SECTION("Deep tree with condense") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values(100);
        std::vector<Rectangle> rects;
        for (int i = 0; i < 100; i++) {
            values[i] = i;
            double cluster_x = (i / 25) * 10.0;
            double cluster_y = (i % 25) * 0.5;
            double x = cluster_x + (i % 5) * 0.1;
            double y = cluster_y;
            rects.push_back(makeRect({x, y}, {x + 0.05, y + 0.05}));
            tree.insert(rects[i], &values[i]);
        }
        std::vector<int> to_delete = {0,  1,  2,  3,  4,  25, 26, 27, 28, 29,
                                      50, 51, 52, 53, 54, 75, 76, 77, 78, 79};
        for (int idx : to_delete) tree.remove(rects[idx]);

        std::vector<int*> results = tree.search(makeRect({-10, -10}, {50, 50}));
        REQUIRE(results.size() == 80);
    }

    SECTION("Extreme condense") {
        Gutman::RTree<int> tree(2, 4);
        std::vector<int> values(200);
        std::vector<Rectangle> rects;
        for (int i = 0; i < 200; i++) {
            values[i] = i;
            int cluster = i / 20;
            int within_cluster = i % 20;
            double x = cluster * 5.0 + (within_cluster % 4) * 0.1;
            double y = cluster * 5.0 + (within_cluster / 4) * 0.1;
            rects.push_back(makeRect({x, y}, {x + 0.05, y + 0.05}));
            tree.insert(rects[i], &values[i]);
        }
        for (int cluster = 0; cluster < 10; cluster += 2)
            for (int j = 0; j < 20; j++) tree.remove(rects[cluster * 20 + j]);

        std::vector<int*> results = tree.search(makeRect({-5, -5}, {60, 60}));
        REQUIRE(results.size() == 100);
    }

    SECTION("Sequential delete and reinsert - STRESS TEST") {
        const int N = 50000;    // number of rectangles
        const int CYCLES = 20;  // number of deletion/reinsertion cycles
        const int STRIDE = 7;   // stride for deletion pattern (was 5)
        const int MIN_CHILD = 8;
        const int MAX_CHILD = 16;

        Gutman::RTree<int> tree(MIN_CHILD, MAX_CHILD);

        std::vector<int> values(N);
        std::vector<Rectangle> rects;
        rects.reserve(N);

        // Create deterministic grid layout
        int grid = std::ceil(std::sqrt(N));
        for (int i = 0; i < N; i++) {
            values[i] = i;
            double x = (i % grid) * 1.2;
            double y = (i / grid) * 1.2;

            rects.push_back(makeRect({x, y}, {x + 1.0, y + 1.0}));
            tree.insert(rects[i], &values[i]);
        }

        bool all_passed = true;

        for (int cycle = 0; cycle < CYCLES; cycle++) {
            std::vector<int> deleted;
            deleted.reserve(N / STRIDE);

            // Sequential pattern with cycle-shift, heavier stride
            for (int i = cycle; i < N; i += STRIDE) {
                tree.remove(rects[i]);
                deleted.push_back(i);
            }

            // Reinsertion (reverse to stress balancing)
            for (int j = (int)deleted.size() - 1; j >= 0; j--) {
                int idx = deleted[j];
                tree.insert(rects[idx], &values[idx]);
            }

            // Global search
            std::vector<int*> results =
                tree.search(makeRect({-1000, -1000}, {(double)grid * 2.0, (double)grid * 2.0}));

            if (results.size() != N)
                all_passed = false;
        }

        REQUIRE(all_passed);
    }

    SECTION("Massive deletions with reinsertion - STRESS TEST") {
        const int N = 50000;         // total inserts
        const int DELETE_N = 30000;  // how many to delete
        const int MIN_CHILD = 8;     // make tree fuller for stress
        const int MAX_CHILD = 16;

        Gutman::RTree<int> tree(MIN_CHILD, MAX_CHILD);
        std::vector<int> values(N);
        std::vector<Rectangle> rects;
        rects.reserve(N);

        // Insert N rectangles in a grid
        int grid = std::ceil(std::sqrt(N));
        for (int i = 0; i < N; i++) {
            values[i] = i;
            double x = (i % grid) * 1.5;
            double y = (i / grid) * 1.5;

            rects.push_back(makeRect({x, y}, {x + 0.8, y + 0.8}));
            tree.insert(rects[i], &values[i]);
        }

        // Massive deletion
        for (int i = 0; i < DELETE_N; i++) {
            tree.remove(rects[i]);
        }

        // Reinsertion of deleted rects in reverse order
        for (int i = DELETE_N - 1; i >= 0; i--) {
            tree.insert(rects[i], &values[i]);
        }

        // Global query
        std::vector<int*> results = tree.search(makeRect({-1000, -1000}, {1000, 1000}));

        REQUIRE(results.size() == N);
    }
}
