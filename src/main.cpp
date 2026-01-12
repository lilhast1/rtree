#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "rtree/rtree.h"
#include "rtree_hilbert/hilbert_rtree.h"
// Assuming the Gutman::RTree class is included here
// #include "rtree.h"

using Rectangle = Gutman::Rectangle;

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
        Gutman::RTree<int> tree(2, 5);

        int val = 42;
        auto rect = makeRect({0.0, 0.0}, {1.0, 1.0});
        tree.insert(rect, &val);

        std::vector<int*> results = tree.search(rect);

        assert_true(results.size() == 1 && *results[0] == 42, "Insert single element");
    }

    void test_insert_multiple_elements() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values = {1, 2, 3, 4, 5};
        std::vector<Rectangle> rects;

        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }

        auto search_rect = makeRect({0.0, 0.0}, {10.0, 10.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 5, "Insert multiple elements");
    }

    void test_insert_overlapping_rectangles() {
        Gutman::RTree<int> tree(2, 4);

        int val1 = 10, val2 = 20, val3 = 30;
        auto rect1 = makeRect({0.0, 0.0}, {5.0, 5.0});
        auto rect2 = makeRect({3.0, 3.0}, {8.0, 8.0});
        auto rect3 = makeRect({4.0, 4.0}, {6.0, 6.0});

        tree.insert(rect1, &val1);
        tree.insert(rect2, &val2);
        tree.insert(rect3, &val3);

        // Search in overlapping region
        auto search_rect = makeRect({4.0, 4.0}, {5.0, 5.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 3, "Insert overlapping rectangles");
    }

    void test_insert_trigger_split() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values(10);
        for (int i = 0; i < 10; i++) {
            values[i] = i;
            auto rect = makeRect({(double)i, (double)i}, {(double)i + 0.5, (double)i + 0.5});
            tree.insert(rect, &values[i]);
        }

        // Search all
        auto search_rect = makeRect({-1.0, -1.0}, {20.0, 20.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 10, "Insert triggering node split");
    }

    // Search Tests
    void test_search_empty_tree() {
        Gutman::RTree<int> tree(2, 4);

        auto search_rect = makeRect({0.0, 0.0}, {10.0, 10.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 0, "Search in empty tree");
    }

    void test_search_no_overlap() {
        Gutman::RTree<int> tree(2, 4);

        int val = 42;
        auto rect = makeRect({0.0, 0.0}, {1.0, 1.0});
        tree.insert(rect, &val);

        auto search_rect = makeRect({10.0, 10.0}, {20.0, 20.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 0, "Search with no overlap");
    }

    void test_search_partial_overlap() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values = {1, 2, 3, 4, 5};
        tree.insert(makeRect({0.0, 0.0}, {2.0, 2.0}), &values[0]);
        tree.insert(makeRect({5.0, 5.0}, {7.0, 7.0}), &values[1]);
        tree.insert(makeRect({10.0, 10.0}, {12.0, 12.0}), &values[2]);
        tree.insert(makeRect({1.0, 1.0}, {3.0, 3.0}), &values[3]);
        tree.insert(makeRect({8.0, 8.0}, {9.0, 9.0}), &values[4]);

        // Search should find only first two
        auto search_rect = makeRect({0.0, 0.0}, {6.0, 6.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 3, "Search with partial overlap");
    }

    void test_search_point_query() {
        Gutman::RTree<int> tree(2, 4);

        int val = 99;
        auto rect = makeRect({5.0, 5.0}, {10.0, 10.0});
        tree.insert(rect, &val);

        // Point inside
        auto point = makeRect({7.0, 7.0}, {7.0, 7.0});
        std::vector<int*> results = tree.search(point);

        assert_true(results.size() == 1 && *results[0] == 99, "Point query inside rectangle");
    }

    // Deletion Tests
    void test_delete_single_element() {
        Gutman::RTree<int> tree(2, 4);

        int val = 42;
        auto rect = makeRect({0.0, 0.0}, {1.0, 1.0});
        tree.insert(rect, &val);
        tree.remove(rect);

        std::vector<int*> results = tree.search(rect);

        assert_true(results.size() == 0, "Delete single element");
    }

    void test_delete_from_multiple() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values = {1, 2, 3, 4, 5};
        std::vector<Rectangle> rects;

        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }

        // Delete middle element
        tree.remove(rects[2]);

        auto search_rect = makeRect({0.0, 0.0}, {10.0, 10.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 4, "Delete from multiple elements");
    }

    void test_delete_nonexistent() {
        Gutman::RTree<int> tree(2, 4);

        int val = 42;
        auto rect1 = makeRect({0.0, 0.0}, {1.0, 1.0});
        auto rect2 = makeRect({10.0, 10.0}, {11.0, 11.0});

        tree.insert(rect1, &val);
        tree.remove(rect2);  // Try to delete non-existent

        std::vector<int*> results = tree.search(rect1);

        assert_true(results.size() == 1, "Delete non-existent element");
    }

    void test_delete_and_reinsert() {
        Gutman::RTree<int> tree(2, 4);
        int val1 = 10, val2 = 20;
        auto rect = makeRect({0.0, 0.0}, {5.0, 5.0});

        tree.insert(rect, &val1);
        tree.remove(rect);
        tree.insert(rect, &val2);

        std::vector<int*> results = tree.search(rect);

        assert_true(results.size() == 1 && *results[0] == 20, "Delete and reinsert");
    }

    void test_delete_multiple_sequential() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values = {1, 2, 3, 4, 5, 6, 7, 8};
        std::vector<Rectangle> rects;

        for (size_t i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 1, (double)i + 1}));
            tree.insert(rects[i], &values[i]);
        }

        // Delete several elements
        tree.remove(rects[1]);
        tree.remove(rects[3]);
        tree.remove(rects[5]);

        auto search_rect = makeRect({-1.0, -1.0}, {20.0, 20.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 5, "Delete multiple sequential");
    }

    // Edge Cases
    void test_3d_rectangles() {
        Gutman::RTree<int> tree(2, 4);

        int val = 42;
        auto rect = makeRect({0.0, 0.0, 0.0}, {1.0, 1.0, 1.0});
        tree.insert(rect, &val);

        std::vector<int*> results = tree.search(rect);

        assert_true(results.size() == 1, "3D rectangles");
    }

    void test_high_dimensional() {
        Gutman::RTree<int> tree(2, 4);

        int val = 42;
        auto rect = makeRect({0.0, 0.0, 0.0, 0.0, 0.0}, {1.0, 1.0, 1.0, 1.0, 1.0});
        tree.insert(rect, &val);

        std::vector<int*> results = tree.search(rect);

        assert_true(results.size() == 1, "High dimensional (5D)");
    }

    void test_zero_area_rectangle() {
        Gutman::RTree<int> tree(2, 4);

        int val = 42;
        auto rect = makeRect({5.0, 5.0}, {5.0, 5.0});
        tree.insert(rect, &val);

        std::vector<int*> results = tree.search(rect);

        assert_true(results.size() == 1, "Zero area rectangle (point)");
    }
    void test_insert_and_search_large_dataset() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values(100);
        for (int i = 0; i < 100; i++) {
            values[i] = i;
            double x = (i % 10) * 2.0;
            double y = (i / 10) * 2.0;
            auto rect = makeRect({x, y}, {x + 1.5, y + 1.5});
            tree.insert(rect, &values[i]);
        }

        auto search_rect = makeRect({-1.0, -1.0}, {30.0, 30.0});
        std::vector<int*> results = tree.search(search_rect);
        assert_true(results.size() == 100, "Insert and search large dataset (100 elements)");
    }

    void test_delete_every_other_element() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values(20);
        std::vector<Rectangle> rects;

        for (int i = 0; i < 20; i++) {
            values[i] = i;
            rects.push_back(makeRect({(double)i, (double)i}, {(double)i + 0.8, (double)i + 0.8}));
            tree.insert(rects[i], &values[i]);
        }

        for (int i = 0; i < 20; i += 2) {
            tree.remove(rects[i]);
        }

        auto search_rect = makeRect({-1.0, -1.0}, {25.0, 25.0});
        std::vector<int*> results = tree.search(search_rect);
        assert_true(results.size() == 10, "Delete every other element");
    }

    void test_search_with_exact_boundaries() {
        Gutman::RTree<int> tree(2, 4);

        int val1 = 10, val2 = 20, val3 = 30;
        auto rect1 = makeRect({0.0, 0.0}, {5.0, 5.0});
        auto rect2 = makeRect({5.0, 5.0}, {10.0, 10.0});
        auto rect3 = makeRect({10.0, 10.0}, {15.0, 15.0});

        tree.insert(rect1, &val1);
        tree.insert(rect2, &val2);
        tree.insert(rect3, &val3);

        auto search_rect = makeRect({0.0, 0.0}, {5.0, 5.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() >= 1, "Search with exact boundaries");
    }

    void test_insert_identical_rectangles() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values = {1, 2, 3, 4, 5};
        auto rect = makeRect({5.0, 5.0}, {10.0, 10.0});

        for (size_t i = 0; i < values.size(); i++) {
            tree.insert(rect, &values[i]);
        }

        std::vector<int*> results = tree.search(rect);

        assert_true(results.size() == 5, "Insert identical rectangles with different values");
    }

    void test_delete_from_single_element_tree() {
        Gutman::RTree<int> tree(2, 4);

        int val = 42;
        auto rect = makeRect({0.0, 0.0}, {1.0, 1.0});

        tree.insert(rect, &val);
        tree.remove(rect);

        int val2 = 99;
        auto rect2 = makeRect({5.0, 5.0}, {6.0, 6.0});
        tree.insert(rect2, &val2);

        std::vector<int*> results = tree.search(rect2);

        assert_true(results.size() == 1 && *results[0] == 99,
                    "Delete from single element tree and reinsert");
    }

    void test_mixed_insert_delete_operations() {
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

        auto search_rect = makeRect({-1.0, -1.0}, {20.0, 20.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 10, "Mixed insert/delete operations");
    }

    void test_stress_test_splits() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values(50);

        for (int i = 0; i < 50; i++) {
            values[i] = i;
            double base_x = (i / 5) * 3.0;
            double base_y = (i % 5) * 3.0;
            auto rect = makeRect({base_x, base_y}, {base_x + 2.0, base_y + 2.0});
            tree.insert(rect, &values[i]);
        }

        auto search_rect = makeRect({-5.0, -5.0}, {50.0, 50.0});
        std::vector<int*> results = tree.search(search_rect);

        auto cluster_rect = makeRect({0.0, 0.0}, {5.0, 5.0});
        std::vector<int*> cluster_results = tree.search(cluster_rect);

        assert_true(results.size() == 50 && cluster_results.size() > 0,
                    "Stress test with multiple splits");
    }
    void test_deep_tree_with_condense() {
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

        for (int idx : to_delete) {
            tree.remove(rects[idx]);
        }

        auto search_rect = makeRect({-10.0, -10.0}, {50.0, 50.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 80, "Deep tree with non-leaf orphans condense");
    }
    hilbert::Rectangle makeRectHilbert(std::vector<long long> min, std::vector<long long> max) {
        return hilbert::Rectangle(min, max);
    }

    void test_deep_tree_with_extreme_condense1() {
        std::cout << "\n[DEBUG] Pokrecem Extreme Condense Test (HILBERT)..." << std::endl;

        // Parametri: min=10, max=40, dims=2, bits=64
        hilbert::RTree<int> tree(10, 40, 2, 64);

        std::vector<int> values(2000);
        std::vector<hilbert::Rectangle> rects;

        // 1. GENERISANJE PODATAKA (Skalirano x100)
        for (int i = 0; i < 2000; i++) {
            values[i] = i;
            int cluster_id = i / 20;
            int within_cluster = i % 20;

            // Množimo sa 100 da bi 0.1 postalo 10, a 5.0 postalo 500
            // Inače bi integer divizija ovo pretvorila u 0
            long long x = (cluster_id * 500) + (within_cluster % 4) * 10;
            long long y = (cluster_id * 500) + (within_cluster / 4) * 10;

            // Width je bio 0.05 -> sada je 5
            auto rect = makeRectHilbert({x, y}, {x + 5, y + 5});

            tree.insert(rect, &values[i]);
            rects.push_back(rect);
        }
        std::cout << "[DEBUG] Insertovano 2000 elemenata." << std::endl;

        // 2. BRISANJE
        // Brišemo svaki drugi klaster u prvih 10 klastera
        // Klasteri 0, 2, 4, 6, 8 se brišu (5 klastera * 20 elemenata = 100 obrisano)
        int removed_count = 0;
        for (int cluster = 0; cluster < 10; cluster += 2) {
            for (int j = 0; j < 20; j++) {
                int idx = cluster * 20 + j;
                tree.remove(rects[idx]);
                removed_count++;
            }
        }
        std::cout << "[DEBUG] Obrisano " << removed_count << " elemenata." << std::endl;

        // 3. PRETRAGA
        // Search rect mora da pokrije prvih 10-12 klastera da bismo videli šta je ostalo
        // Original: -5.0 do 60.0 -> Skalirano: -500 do 6000
        auto search_rect = makeRectHilbert({-500, -500}, {4900, 4900});

        std::deque<int*> results = tree.search(search_rect);
        size_t found = results.size();

        // OČEKIVANJE:
        // Search rect pokriva klastere od 0 do 11 (grubo).
        // Klasteri 0, 2, 4, 6, 8 su OBRISANI.
        // Klasteri 1, 3, 5, 7, 9 su OSTALI.
        // Ukupno očekujemo: 5 klastera * 20 elemenata = 100.
        size_t expected = 100;

        if (found == expected) {
            std::cout << "✓ PASS: Extreme condense (Nadjeno: " << found << ")" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAIL: Extreme condense" << std::endl;
            std::cout << "    -> Ocekivano: " << expected << std::endl;
            std::cout << "    -> Nadjeno:   " << found << std::endl;
            // Ispis prvih nekoliko da vidimo sta je ostalo (opciono)
            if (found > 0)
                std::cout << "    -> Primer vrednosti: " << *results[0] << std::endl;
            failed++;
        }
    }
    void test_deep_tree_with_extreme_condense() {
        std::cout << "\n[DEBUG] Pokrecem Extreme Condense Test..." << std::endl;

        // PAŽNJA: Proveri da li testiraš Gutman ili Hilbert!
        // Ako je Hilbert, koordinate moraju biti skalirane (vidi napomenu ispod)
        // Ovde ostavljam Gutman kako je u tvom primeru, ali promeni tip ako testiraš Hilbert.
        Gutman::RTree<int> tree(10, 40);
        // Za Hilbert bi bilo: hilbert::RTree<int> tree(10, 40, 2, 64);

        std::vector<int> values(2000);
        std::vector<Rectangle> rects;

        for (int i = 0; i < 2000; i++) {
            values[i] = i;
            int cluster_id = i / 20;
            int within_cluster = i % 20;

            double x = cluster_id * 5.0 + (within_cluster % 4) * 0.1;
            double y = cluster_id * 5.0 + (within_cluster / 4) * 0.1;

            rects.push_back(makeRect({x, y}, {x + 0.05, y + 0.05}));
            tree.insert(rects[i], &values[i]);
        }

        std::cout << "[DEBUG] Insertovano 2000 elemenata." << std::endl;

        int removed_count = 0;
        for (int cluster = 0; cluster < 10; cluster += 2) {
            for (int j = 0; j < 20; j++) {
                int idx = cluster * 20 + j;
                tree.remove(rects[idx]);
                removed_count++;
            }
        }
        std::cout << "[DEBUG] Obrisano " << removed_count << " elemenata (ocekivano 100)."
                  << std::endl;

        auto search_rect = makeRect({-5.0, -5.0}, {49.0, 49.0});
        std::vector<int*> results = tree.search(search_rect);

        size_t found = results.size();
        size_t expected = 100;
        if (found == expected) {
            std::cout << "✓ PASS: Extreme condense (Nadjeno: " << found << ")" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAIL: Extreme condense" << std::endl;
            std::cout << "    -> Ukupno ubaceno: 2000" << std::endl;
            std::cout << "    -> Ocekivano da ostane: " << expected << std::endl;
            std::cout << "    -> Stvarno pronadjeno:  " << found << std::endl;
            std::cout << "    -> Razlika: " << (long long)found - (long long)expected << std::endl;
            failed++;
        }
    }

    void test_extra_delete_and_reinsert() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values(50);
        std::vector<Rectangle> rects;

        for (int i = 0; i < 50; i++) {
            values[i] = i;
            double x = (i % 7) * 2.0;
            double y = (i / 7) * 2.0;
            rects.push_back(makeRect({x, y}, {x + 1.0, y + 1.0}));
            tree.insert(rects[i], &values[i]);
        }

        bool all_passed = true;
        for (int cycle = 0; cycle < 3; cycle++) {
            std::vector<int> deleted_indices;
            for (int i = cycle; i < 50; i += 5) {
                tree.remove(rects[i]);
                deleted_indices.push_back(i);
            }

            for (int idx : deleted_indices) {
                tree.insert(rects[idx], &values[idx]);
            }

            auto search_rect = makeRect({-5.0, -5.0}, {20.0, 20.0});
            std::vector<int*> results = tree.search(search_rect);

            if (results.size() != 50) {
                all_passed = false;
                break;
            }
        }

        assert_true(all_passed, "Sequential delete and reinsert");
    }

    void test_massive_delete_and_reinsert() {
        Gutman::RTree<int> tree(2, 4);

        std::vector<int> values(150);
        std::vector<Rectangle> rects;

        for (int i = 0; i < 150; i++) {
            values[i] = i;
            double x = (i % 12) * 1.5;
            double y = (i / 12) * 1.5;
            rects.push_back(makeRect({x, y}, {x + 0.8, y + 0.8}));
            tree.insert(rects[i], &values[i]);
        }

        for (int i = 0; i < 100; i++) {
            tree.remove(rects[i]);
        }

        auto search_rect = makeRect({-10.0, -10.0}, {50.0, 50.0});
        std::vector<int*> results = tree.search(search_rect);

        assert_true(results.size() == 50, "Massive deletions with reinsertion");
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
        test_search_with_exact_boundaries();

        std::cout << "\n--- Deletion Tests ---" << std::endl;
        test_delete_single_element();
        test_delete_from_multiple();
        test_delete_nonexistent();
        test_delete_and_reinsert();
        test_delete_multiple_sequential();
        test_delete_every_other_element();
        test_delete_from_single_element_tree();

        std::cout << "\n--- Edge Cases ---" << std::endl;
        test_3d_rectangles();
        test_high_dimensional();
        test_zero_area_rectangle();

        std::cout << "\n--- Stress Tests ---" << std::endl;
        test_insert_and_search_large_dataset();
        test_mixed_insert_delete_operations();
        test_stress_test_splits();

        std::cout << "\n--- Condense Tree Tests ---" << std::endl;
        test_deep_tree_with_condense();
        test_deep_tree_with_extreme_condense1();
        test_deep_tree_with_extreme_condense();
        test_extra_delete_and_reinsert();
        test_massive_delete_and_reinsert();

        print_summary();
    }
};
using Payload = int;

// Struktura za učitane podatke
struct DataPoint {
    long long x;
    long long y;
    Payload id;
};

// Pomoćna funkcija za merenje vremena
template <typename Func>
double measure_time(Func&& func) {
    auto start = std::chrono::high_resolution_clock::now();
    func();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    return diff.count();
}

// Funkcija za učitavanje podataka i skaliranje
std::vector<DataPoint> load_dataset(const std::string& filename) {
    std::vector<DataPoint> data;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "GRESKA: Ne mogu da otvorim fajl: " << filename << std::endl;
        std::cerr << "Proveri da li je fajl u istom folderu kao .exe!" << std::endl;
        return data;
    }

    double lat, lon;
    int id = 0;
    while (file >> lat >> lon) {
        // SKALIRANJE: Množimo sa 100 i kastujemo u long long
        long long x = static_cast<long long>(lat * 100.0);
        long long y = static_cast<long long>(lon * 100.0);
        data.push_back({x, y, id++});
    }

    std::cout << "Ucitano " << data.size() << " tacaka iz " << filename << std::endl;
    return data;
}

// Pronalazi granice (MBR) celog dataseta za potrebe pretrage
void get_dataset_bounds(const std::vector<DataPoint>& data, long long& min_x, long long& min_y,
                        long long& max_x, long long& max_y) {
    min_x = std::numeric_limits<long long>::max();
    min_y = std::numeric_limits<long long>::max();
    max_x = std::numeric_limits<long long>::min();
    max_y = std::numeric_limits<long long>::min();

    for (const auto& p : data) {
        if (p.x < min_x)
            min_x = p.x;
        if (p.y < min_y)
            min_y = p.y;
        if (p.x > max_x)
            max_x = p.x;
        if (p.y > max_y)
            max_y = p.y;
    }
}

void run_benchmark(const std::string& dataset_name, const std::string& filename) {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "BENCHMARK: " << dataset_name << std::endl;
    std::cout << "========================================================" << std::endl;

    auto data = load_dataset(filename);
    if (data.empty())
        return;

    size_t total_points = data.size();  // Koliko tacaka imamo ukupno

    long long min_x, min_y, max_x, max_y;
    get_dataset_bounds(data, min_x, min_y, max_x, max_y);

    int min_entries = 4;
    int max_entries = 8;

    // Promenljive za cuvanje rezultata da ih uporedimo na kraju
    size_t gutman_found = 0;
    size_t hilbert_found = 0;

    // -------------------------------------------------
    // 1. GUTMAN
    // -------------------------------------------------
    {
        std::cout << "\n--- Gutman R-Tree ---" << std::endl;
        Gutman::RTree<Payload> tree(min_entries, max_entries);

        double t_insert = measure_time([&]() {
            for (const auto& p : data) {
                std::vector<double> p_min = {static_cast<double>(p.x), static_cast<double>(p.y)};
                std::vector<double> p_max = {static_cast<double>(p.x), static_cast<double>(p.y)};
                tree.insert(Gutman::Rectangle(p_min, p_max), new Payload(p.id));
            }
        });
        std::cout << "Insert Time: " << std::fixed << std::setprecision(6) << t_insert << " s"
                  << std::endl;

        double t_search = measure_time([&]() {
            std::vector<double> s_min = {static_cast<double>(min_x), static_cast<double>(min_y)};
            std::vector<double> s_max = {static_cast<double>(max_x), static_cast<double>(max_y)};
            auto results = tree.search(Gutman::Rectangle(s_min, s_max));
            gutman_found = results.size();  // <--- CUVAMO REZULTAT
        });
        std::cout << "Search Time: " << std::fixed << std::setprecision(6) << t_search << " s"
                  << std::endl;
        std::cout << "Pronadjeno tacaka: " << gutman_found << " / " << total_points << std::endl;
    }

    // -------------------------------------------------
    // 2. HILBERT
    // -------------------------------------------------
    {
        std::cout << "\n--- Hilbert R-Tree ---" << std::endl;
        hilbert::RTree<Payload> tree(min_entries, max_entries, 2, 64);

        double t_insert = measure_time([&]() {
            for (const auto& p : data) {
                std::vector<long long> p_min = {p.x, p.y};
                std::vector<long long> p_max = {p.x, p.y};
                tree.insert(hilbert::Rectangle(p_min, p_max), new Payload(p.id));
            }
        });
        std::cout << "Insert Time: " << std::fixed << std::setprecision(6) << t_insert << " s"
                  << std::endl;

        double t_search = measure_time([&]() {
            std::vector<long long> s_min = {min_x, min_y};
            std::vector<long long> s_max = {max_x, max_y};
            auto results = tree.search(hilbert::Rectangle(s_min, s_max));
            hilbert_found = results.size();  // <--- CUVAMO REZULTAT
        });
        std::cout << "Search Time: " << std::fixed << std::setprecision(6) << t_search << " s"
                  << std::endl;
        std::cout << "Pronadjeno tacaka: " << hilbert_found << " / " << total_points << std::endl;
    }

    // -------------------------------------------------
    // ZAKLJUCAK / VALIDACIJA
    // -------------------------------------------------
    std::cout << "\n--- VALIDACIJA ---" << std::endl;
    if (gutman_found == total_points && hilbert_found == total_points) {
        std::cout << "[SUCCESS] Oba stabla su pronasla sve tacke! Podaci su validni." << std::endl;
    } else {
        std::cout << "[FAIL] Greska u podacima!" << std::endl;
        if (gutman_found != total_points)
            std::cout << "Gutman fali: " << (total_points - gutman_found) << std::endl;
        if (hilbert_found != total_points)
            std::cout << "Hilbert fali: " << (total_points - hilbert_found) << std::endl;
    }
}
void run_scalability_test(const std::string& filename) {
    std::cout << "\n========================================================" << std::endl;
    std::cout << "GENERISANJE PODATAKA ZA GRAFIKE (SKALABILNOST)" << std::endl;
    std::cout << "========================================================" << std::endl;

    auto full_data = load_dataset(filename);
    if (full_data.empty())
        return;

    // Definisemo korake za grafik (npr. 5k, 10k, 20k... do max)
    std::vector<size_t> steps = {5000, 10000, 15000, 20000, 25000, 30000, 35000, full_data.size()};

    // Otvaramo fajl za snimanje rezultata (CSV format)
    std::ofstream csv_file("benchmark_results.csv");
    csv_file << "N,GutmanInsert,HilbertInsert,GutmanSearch,HilbertSearch\n";  // Header

    std::cout << "N\tG_Ins\tH_Ins\tG_Srch\tH_Srch" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    for (size_t n : steps) {
        if (n > full_data.size())
            n = full_data.size();

        // 1. Priprema podskupa podataka (prvih N tacaka)
        std::vector<DataPoint> subset(full_data.begin(), full_data.begin() + n);

        // Nadjemo granice za ovaj podskup da bi pretraga bila fer
        long long min_x, min_y, max_x, max_y;
        get_dataset_bounds(subset, min_x, min_y, max_x, max_y);

        // Varijable za vremena
        double g_insert = 0, h_insert = 0;
        double g_search = 0, h_search = 0;

        // --- GUTMAN MERENJE ---
        {
            Gutman::RTree<Payload> tree(4, 8);
            g_insert = measure_time([&]() {
                for (const auto& p : subset) {
                    std::vector<double> p_min = {static_cast<double>(p.x),
                                                 static_cast<double>(p.y)};
                    std::vector<double> p_max = {static_cast<double>(p.x),
                                                 static_cast<double>(p.y)};
                    tree.insert(Gutman::Rectangle(p_min, p_max), new Payload(p.id));
                }
            });

            g_search = measure_time([&]() {
                std::vector<double> s_min = {static_cast<double>(min_x),
                                             static_cast<double>(min_y)};
                std::vector<double> s_max = {static_cast<double>(max_x),
                                             static_cast<double>(max_y)};
                auto res = tree.search(Gutman::Rectangle(s_min, s_max));
            });
        }

        // --- HILBERT MERENJE ---
        {
            hilbert::RTree<Payload> tree(4, 8, 2, 64);
            h_insert = measure_time([&]() {
                for (const auto& p : subset) {
                    std::vector<long long> p_min = {p.x, p.y};
                    std::vector<long long> p_max = {p.x, p.y};
                    tree.insert(hilbert::Rectangle(p_min, p_max), new Payload(p.id));
                }
            });

            h_search = measure_time([&]() {
                std::vector<long long> s_min = {min_x, min_y};
                std::vector<long long> s_max = {max_x, max_y};
                auto res = tree.search(hilbert::Rectangle(s_min, s_max));
            });
        }

        // Ispis na ekran da vidis da radi
        std::cout << n << "\t" << std::fixed << std::setprecision(3) << g_insert << "\t" << h_insert
                  << "\t" << std::setprecision(5) << g_search << "\t" << h_search << std::endl;

        // Upis u CSV fajl
        csv_file << n << "," << std::fixed << std::setprecision(6) << g_insert << "," << h_insert
                 << "," << g_search << "," << h_search << "\n";
    }

    std::cout << "\n[INFO] Rezultati sacuvani u 'benchmark_results.csv'" << std::endl;
}
int main() {
    RTreeTest test_suite;
    test_suite.run_all_tests();
    run_benchmark("1000 Points Dataset", "1000.txt");
    run_benchmark("Greek Earthquakes (1964-2000)", "greek-earthquakes-1964-2000.txt");
    run_scalability_test("greek-earthquakes-1964-2000.txt");
    std::cout << "\nBenchmark zavrsen." << std::endl;
    return 0;
}