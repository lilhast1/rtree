#include <iostream>

#include "rtree/rtree.h"

#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>

// Assuming the RTreeGutman class is included here
// #include "rtree.h"

using Rectangle = RTreeGutman<int>::Rectangle;

// Helper function to create a rectangle
Rectangle makeRect(std::vector<double> min, std::vector<double> max) {
    return Rectangle(min, max);
}

// Test fixture class
class RTreeTest {
private:
    int passed = 0;
    int failed = 0;
    
public:
    void assert_true(bool condition, const std::string& test_name) {
        if (condition) {
            std::cout << "✓ PASS: " << test_name << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAIL: " << test_name << std::endl;
            failed++;
        }
    }
    
    void print_summary() {
        std::cout << "\n========== Test Summary ==========" << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Total:  " << (passed + failed) << std::endl;
        std::cout << "==================================" << std::endl;
    }
    
    // Basic Insertion Tests
    void test_insert_single_element() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 5;
        
        int val = 42;
        auto rect = makeRect({0.0, 0.0}, {1.0, 1.0});
        tree.insert(rect, &val);
        
        std::vector<int*> results;
        tree.search(rect, results);
        
        assert_true(results.size() == 1 && *results[0] == 42, 
                   "Insert single element");
    }
    
    void test_insert_multiple_elements() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        std::vector<int> values = {1, 2, 3, 4, 5};
        std::vector<Rectangle> rects;
        
        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, 
                                    {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        
        std::vector<int*> results;
        auto search_rect = makeRect({0.0, 0.0}, {10.0, 10.0});
        tree.search(search_rect, results);
        
        assert_true(results.size() == 5, "Insert multiple elements");
    }
    
    void test_insert_overlapping_rectangles() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val1 = 10, val2 = 20, val3 = 30;
        auto rect1 = makeRect({0.0, 0.0}, {5.0, 5.0});
        auto rect2 = makeRect({3.0, 3.0}, {8.0, 8.0});
        auto rect3 = makeRect({4.0, 4.0}, {6.0, 6.0});
        
        tree.insert(rect1, &val1);
        tree.insert(rect2, &val2);
        tree.insert(rect3, &val3);
        
        // Search in overlapping region
        auto search_rect = makeRect({4.0, 4.0}, {5.0, 5.0});
        std::vector<int*> results;
        tree.search(search_rect, results);
        
        assert_true(results.size() == 3, "Insert overlapping rectangles");
    }
    
    void test_insert_trigger_split() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        std::vector<int> values(10);
        for (int i = 0; i < 10; i++) {
            values[i] = i;
            auto rect = makeRect({(double)i, (double)i}, 
                                {(double)i + 0.5, (double)i + 0.5});
            tree.insert(rect, &values[i]);
        }
        
        // Search all
        auto search_rect = makeRect({-1.0, -1.0}, {20.0, 20.0});
        std::vector<int*> results;
        tree.search(search_rect, results);
        
        assert_true(results.size() == 10, "Insert triggering node split");
    }
    
    // Search Tests
    void test_search_empty_tree() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        auto search_rect = makeRect({0.0, 0.0}, {10.0, 10.0});
        std::vector<int*> results;
        tree.search(search_rect, results);
        
        assert_true(results.size() == 0, "Search in empty tree");
    }
    
    void test_search_no_overlap() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val = 42;
        auto rect = makeRect({0.0, 0.0}, {1.0, 1.0});
        tree.insert(rect, &val);
        
        auto search_rect = makeRect({10.0, 10.0}, {20.0, 20.0});
        std::vector<int*> results;
        tree.search(search_rect, results);
        
        assert_true(results.size() == 0, "Search with no overlap");
    }
    
    void test_search_partial_overlap() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        std::vector<int> values = {1, 2, 3, 4, 5};
        tree.insert(makeRect({0.0, 0.0}, {2.0, 2.0}), &values[0]);
        tree.insert(makeRect({5.0, 5.0}, {7.0, 7.0}), &values[1]);
        tree.insert(makeRect({10.0, 10.0}, {12.0, 12.0}), &values[2]);
        tree.insert(makeRect({1.0, 1.0}, {3.0, 3.0}), &values[3]);
        tree.insert(makeRect({8.0, 8.0}, {9.0, 9.0}), &values[4]);
        
        // Search should find only first two
        auto search_rect = makeRect({0.0, 0.0}, {6.0, 6.0});
        std::vector<int*> results;
        tree.search(search_rect, results);
        
        assert_true(results.size() == 3, "Search with partial overlap");
    }
    
    void test_search_point_query() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val = 99;
        auto rect = makeRect({5.0, 5.0}, {10.0, 10.0});
        tree.insert(rect, &val);
        
        // Point inside
        auto point = makeRect({7.0, 7.0}, {7.0, 7.0});
        std::vector<int*> results;
        tree.search(point, results);
        
        assert_true(results.size() == 1 && *results[0] == 99, 
                   "Point query inside rectangle");
    }
    
    // Deletion Tests
    void test_delete_single_element() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val = 42;
        auto rect = makeRect({0.0, 0.0}, {1.0, 1.0});
        tree.insert(rect, &val);
        tree.remove(rect);
        
        std::vector<int*> results;
        tree.search(rect, results);
        
        assert_true(results.size() == 0, "Delete single element");
    }
    
    void test_delete_from_multiple() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        std::vector<int> values = {1, 2, 3, 4, 5};
        std::vector<Rectangle> rects;
        
        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, 
                                    {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        
        // Delete middle element
        tree.remove(rects[2]);
        
        std::vector<int*> results;
        auto search_rect = makeRect({0.0, 0.0}, {10.0, 10.0});
        tree.search(search_rect, results);
        
        assert_true(results.size() == 4, "Delete from multiple elements");
    }
    
    void test_delete_nonexistent() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val = 42;
        auto rect1 = makeRect({0.0, 0.0}, {1.0, 1.0});
        auto rect2 = makeRect({10.0, 10.0}, {11.0, 11.0});
        
        tree.insert(rect1, &val);
        tree.remove(rect2); // Try to delete non-existent
        
        std::vector<int*> results;
        tree.search(rect1, results);
        
        assert_true(results.size() == 1, "Delete non-existent element");
    }
    
    void test_delete_and_reinsert() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val1 = 10, val2 = 20;
        auto rect = makeRect({0.0, 0.0}, {5.0, 5.0});
        
        tree.insert(rect, &val1);
        tree.remove(rect);
        tree.insert(rect, &val2);
        
        std::vector<int*> results;
        tree.search(rect, results);
        
        assert_true(results.size() == 1 && *results[0] == 20, 
                   "Delete and reinsert");
    }
    
    void test_delete_multiple_sequential() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        std::vector<int> values = {1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<Rectangle> rects;
        
        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, 
                                    {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        
        // Delete several elements
        tree.remove(rects[1]);
        tree.remove(rects[3]);
        tree.remove(rects[5]);
        
        std::vector<int*> results;
        auto search_rect = makeRect({-1.0, -1.0}, {20.0, 20.0});
        tree.search(search_rect, results);
        
        assert_true(results.size() == 5, "Delete multiple sequential");
    }
    
    // Edge Cases
    void test_3d_rectangles() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val = 42;
        auto rect = makeRect({0.0, 0.0, 0.0}, {1.0, 1.0, 1.0});
        tree.insert(rect, &val);
        
        std::vector<int*> results;
        tree.search(rect, results);
        
        assert_true(results.size() == 1, "3D rectangles");
    }
    
    void test_high_dimensional() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val = 42;
        auto rect = makeRect({0.0, 0.0, 0.0, 0.0, 0.0}, 
                            {1.0, 1.0, 1.0, 1.0, 1.0});
        tree.insert(rect, &val);
        
        std::vector<int*> results;
        tree.search(rect, results);
        
        assert_true(results.size() == 1, "High dimensional (5D)");
    }
    
    void test_zero_area_rectangle() {
        RTreeGutman<int> tree;
        tree.m = 2;
        tree.M = 4;
        
        int val = 42;
        auto rect = makeRect({5.0, 5.0}, {5.0, 5.0});
        tree.insert(rect, &val);
        
        std::vector<int*> results;
        tree.search(rect, results);
        
        assert_true(results.size() == 1, "Zero area rectangle (point)");
    }
    
    void run_all_tests() {
        std::cout << "\n========== Running R-Tree Tests ==========" << std::endl;
        
        std::cout << "\n--- Insertion Tests ---" << std::endl;
        test_insert_single_element();
        test_insert_multiple_elements();
        test_insert_overlapping_rectangles();
        test_insert_trigger_split();
        
        std::cout << "\n--- Search Tests ---" << std::endl;
        test_search_empty_tree();
        test_search_no_overlap();
        test_search_partial_overlap();
        test_search_point_query();
        
        std::cout << "\n--- Deletion Tests ---" << std::endl;
        test_delete_single_element();
        test_delete_from_multiple();
        test_delete_nonexistent();
        test_delete_and_reinsert();
        test_delete_multiple_sequential();
        
        std::cout << "\n--- Edge Cases ---" << std::endl;
        test_3d_rectangles();
        test_high_dimensional();
        test_zero_area_rectangle();
        
        print_summary();
    }
};

int main() {
    RTreeTest test_suite;
    test_suite.run_all_tests();
    return 0;
}