#include <deque>
#include <vector>

#include "rtree_hilbert/hilbert_curve.h"
#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "rtree_hilbert/hilbert_rtree.h"

// Helper alias (assuming you have makeRect defined somewhere)
// using Rectangle = hilbert::RTree<int>::Rectangle;

// Helper function to create a rectangle
using Rectangle = hilbert::Rectangle;
using ll = long long;
using Point = std::vector<ll>;

// Helper function to create a rectangle
Rectangle makeRect(Point min, Point max) {
    return Rectangle(min, max);
}

// using Rectangle = decltype(makeRect(std::deque<double>{}, std::deque<double>{}));

// ------------------- Insertion Tests -------------------
TEST_CASE("HilbertRTree insertion tests", "[insertion]") {
    SECTION("Insert single element") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 42;
        auto rect = makeRect({0, 0}, {1, 1});
        tree.insert(rect, &val);

        std::deque<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
        REQUIRE(*results[0] == 42);
    }

    SECTION("Insert multiple elements") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values = {1, 2, 3, 4, 5};
        std::deque<Rectangle> rects;
        for (ll i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({i, i}, {i + 1, i + 1}));
            tree.insert(rects[i], &values[i]);
        }

        auto rect = makeRect({0, 0}, {10, 10});
        std::deque<int*> results = tree.search(rect);
        REQUIRE(results.size() == 5);
    }

    SECTION("Insert overlapping rectangles") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val1 = 10, val2 = 20, val3 = 30;
        auto rect1 = makeRect({0, 0}, {5, 5});
        auto rect2 = makeRect({3, 3}, {8, 8});
        auto rect3 = makeRect({4, 4}, {6, 6});
        tree.insert(rect1, &val1);
        tree.insert(rect2, &val2);
        tree.insert(rect3, &val3);

        std::deque<int*> results = tree.search(makeRect({4, 4}, {5, 5}));
        REQUIRE(results.size() == 3);
    }

    SECTION("Insert triggering node split") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values(10);
        for (int i = 0; i < 10; i++) {
            values[i] = i;
            auto rect = makeRect({i, i}, {i, i});
            tree.insert(rect, &values[i]);
        }
        std::deque<int*> results = tree.search(makeRect({-1, -1}, {40, 40}));
        REQUIRE(results.size() == 10);
    }

    SECTION("Insert identical rectangles with different values") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values = {1, 2, 3, 4, 5};
        auto rect = makeRect({5, 5}, {10, 10});
        for (auto& v : values) tree.insert(rect, &v);

        std::deque<int*> results = tree.search(rect);
        REQUIRE(results.size() == 5);
    }

    SECTION("Insert and search large dataset") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values(100);
        for (int i = 0; i < 100; i++) {
            values[i] = i;
            ll x = (i % 10) * 2.0;
            ll y = (i / 10) * 2.0;
            tree.insert(makeRect({x, y}, {x + 2, y + 2}), &values[i]);
        }
        std::deque<int*> results = tree.search(makeRect({-1, -1}, {30, 30}));
        REQUIRE(results.size() == 100);
    }
}

// ------------------- Search Tests -------------------
TEST_CASE("HilbertRTree search tests", "[search]") {
    SECTION("Search empty tree") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int*> results = tree.search(makeRect({0, 0}, {10, 10}));
        REQUIRE(results.size() == 0);
    }

    SECTION("Search no overlap") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 42;
        tree.insert(makeRect({0, 0}, {1, 1}), &val);
        std::deque<int*> results = tree.search(makeRect({10, 10}, {20, 20}));
        REQUIRE(results.size() == 0);
    }

    SECTION("Search partial overlap") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values = {1, 2, 3, 4, 5};
        tree.insert(makeRect({0, 0}, {2, 2}), &values[0]);
        tree.insert(makeRect({5, 5}, {7, 7}), &values[1]);
        tree.insert(makeRect({10, 10}, {12, 12}), &values[2]);
        tree.insert(makeRect({1, 1}, {3, 3}), &values[3]);
        tree.insert(makeRect({8, 8}, {9, 9}), &values[4]);

        std::deque<int*> results = tree.search(makeRect({0, 0}, {6, 6}));
        REQUIRE(results.size() == 3);
    }

    SECTION("Point query") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 99;
        tree.insert(makeRect({5, 5}, {10, 10}), &val);
        std::deque<int*> results = tree.search(makeRect({7, 7}, {7, 7}));
        REQUIRE(results.size() == 1);
        REQUIRE(*results[0] == 99);
    }

    SECTION("Search with exact boundaries") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val1 = 10, val2 = 20, val3 = 30;
        tree.insert(makeRect({0, 0}, {5, 5}), &val1);
        tree.insert(makeRect({5, 5}, {10, 10}), &val2);
        tree.insert(makeRect({10, 10}, {15, 15}), &val3);

        std::deque<int*> results = tree.search(makeRect({0, 0}, {5, 5}));
        REQUIRE(results.size() >= 1);
    }
}

// ------------------- Deletion Tests -------------------
TEST_CASE("HilbertRTree deletion tests", "[deletion]") {
    SECTION("Delete single element") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 42;
        auto rect = makeRect({0, 0}, {1, 1});
        tree.insert(rect, &val);
        tree.remove(rect);

        std::deque<int*> results = tree.search(rect);
        REQUIRE(results.size() == 0);
    }

    SECTION("Delete from multiple elements") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values = {1, 2, 3, 4, 5};
        std::deque<Rectangle> rects;
        for (ll i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({i, i}, {i + 1, i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        tree.remove(rects[2]);
        std::deque<int*> results = tree.search(makeRect({0, 0}, {10, 10}));
        REQUIRE(results.size() == 4);
    }

    SECTION("Delete non-existent element") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 42;
        auto rect1 = makeRect({0, 0}, {1, 1});
        auto rect2 = makeRect({10, 10}, {11, 11});
        tree.insert(rect1, &val);
        tree.remove(rect2);

        std::deque<int*> results = tree.search(rect1);
        REQUIRE(results.size() == 1);
    }

    SECTION("Delete and reinsert") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val1 = 10, val2 = 20;
        auto rect = makeRect({0, 0}, {5, 5});
        tree.insert(rect, &val1);
        tree.remove(rect);
        tree.insert(rect, &val2);

        std::deque<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
        REQUIRE(*results[0] == 20);
    }

    SECTION("Delete multiple sequential elements") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values = {1, 2, 3, 4, 5, 6, 7, 8};
        std::deque<Rectangle> rects;
        for (ll i = 0; i < values.size(); i++) {
            rects.push_back(makeRect({i, i}, {i + 1, i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        tree.remove(rects[1]);
        tree.remove(rects[3]);
        tree.remove(rects[5]);

        std::deque<int*> results = tree.search(makeRect({-1, -1}, {20, 20}));
        REQUIRE(results.size() == 5);
    }

    SECTION("Delete every other element") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values(20);
        std::deque<Rectangle> rects;
        for (int i = 0; i < 20; i++) {
            values[i] = i;
            rects.push_back(makeRect({i, i}, {i + 1, i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        for (int i = 0; i < 20; i += 2) tree.remove(rects[i]);

        std::deque<int*> results = tree.search(makeRect({-1, -1}, {25, 25}));
        REQUIRE(results.size() == 10);
    }

    SECTION("Delete from single element tree and reinsert") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 42;
        auto rect1 = makeRect({0, 0}, {1, 1});
        tree.insert(rect1, &val);
        tree.remove(rect1);

        int val2 = 99;
        auto rect2 = makeRect({5, 5}, {6, 6});
        tree.insert(rect2, &val2);

        std::deque<int*> results = tree.search(rect2);
        REQUIRE(results.size() == 1);
        REQUIRE(*results[0] == 99);
    }
}

// ------------------- Edge Cases -------------------
TEST_CASE("HilbertRTree edge cases", "[edge]") {
    SECTION("3D rectangles") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 42;
        auto rect = makeRect({0, 0, 0}, {1, 1, 1});
        tree.insert(rect, &val);
        std::deque<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
    }

    SECTION("High dimensional 5D") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 42;
        auto rect = makeRect({0, 0, 0, 0, 0}, {1, 1, 1, 1, 1});
        tree.insert(rect, &val);
        std::deque<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
    }

    SECTION("Zero area rectangle") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        int val = 42;
        auto rect = makeRect({5, 5}, {5, 5});
        tree.insert(rect, &val);
        std::deque<int*> results = tree.search(rect);
        REQUIRE(results.size() == 1);
    }
}

// ------------------- Stress / Condense Tests -------------------
TEST_CASE("HilbertRTree stress and condense tests", "[stress]") {
    SECTION("Mixed insert/delete operations") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values(15);
        std::deque<Rectangle> rects;
        for (int i = 0; i < 5; i++) {
            values[i] = i;
            rects.push_back(makeRect({i, i}, {i + 1, i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        tree.remove(rects[1]);
        tree.remove(rects[3]);
        for (int i = 5; i < 10; i++) {
            values[i] = i;
            rects.push_back(makeRect({i, i}, {i + 1, i + 1}));
            tree.insert(rects[i], &values[i]);
        }
        tree.remove(rects[2]);
        tree.remove(rects[6]);
        tree.remove(rects[8]);
        for (int i = 10; i < 15; i++) {
            values[i] = i;
            rects.push_back(makeRect({i, i}, {i + 1, i + 1}));
            tree.insert(rects[i], &values[i]);
        }

        std::deque<int*> results = tree.search(makeRect({-1, -1}, {20, 20}));
        REQUIRE(results.size() == 10);
    }

    SECTION("Stress test with millions of inserts and multiple splits") {
        hilbert::RTree<int> tree(2, 4, 2, 64);

        const size_t N = 1'000'000;  // 1 million
        std::deque<int> values(N);
        // values.reserve(N);

        for (size_t i = 0; i < N; i++) {
            values[i] = static_cast<int>(i);

            // spread rectangles in a large grid,
            // similar logic to your original code
            ll base_x = (i / 1000) * 3.0;  // 1000 per row
            ll base_y = (i % 1000) * 3.0;

            tree.insert(makeRect({base_x, base_y}, {base_x + 2, base_y + 2}), &values[i]);
        }

        // Search bounding region
        auto results = tree.search(makeRect({-5, -5}, {3'500'000, 3'500'000}));

        // Search small cluster
        auto cluster_results = tree.search(makeRect({0, 0}, {5, 5}));

        REQUIRE(results.size() == N);
        REQUIRE(cluster_results.size() > 0);
    }

    SECTION("Deep tree with condense") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values(100);
        std::deque<Rectangle> rects;
        for (int i = 0; i < 100; i++) {
            values[i] = i;
            ll cluster_x = (i / 25) * 10.0;
            ll cluster_y = (i % 25) * 0.5;
            ll x = cluster_x + (i % 5) / 10;
            ll y = cluster_y;
            rects.push_back(makeRect({x, y}, {x + 1, y + 1}));
            tree.insert(rects[i], &values[i]);
        }
        std::deque<int> to_delete = {0,  1,  2,  3,  4,  25, 26, 27, 28, 29,
                                     50, 51, 52, 53, 54, 75, 76, 77, 78, 79};
        for (int idx : to_delete) tree.remove(rects[idx]);

        std::deque<int*> results = tree.search(makeRect({-10, -10}, {50, 50}));
        REQUIRE(results.size() == 80);
    }

    SECTION("Extreme condense") {
        hilbert::RTree<int> tree(2, 4, 2, 64);
        std::deque<int> values(200);
        std::deque<Rectangle> rects;
        for (int i = 0; i < 200; i++) {
            values[i] = i;
            int cluster = i / 20;
            int within_cluster = i % 20;
            ll x = cluster * 5.0 + (within_cluster % 4) * 0.1;
            ll y = cluster * 5.0 + (within_cluster / 4) * 0.1;
            rects.push_back(makeRect({x, y}, {x + 1, y + 1}));
            tree.insert(rects[i], &values[i]);
        }
        for (int cluster = 0; cluster < 10; cluster += 2)
            for (int j = 0; j < 20; j++) tree.remove(rects[cluster * 20 + j]);

        std::deque<int*> results = tree.search(makeRect({-5, -5}, {60, 60}));
        REQUIRE(results.size() == 100);
    }

    SECTION("Sequential delete and reinsert - STRESS TEST") {
        const int N = 50000;    // number of rectangles
        const int CYCLES = 20;  // number of deletion/reinsertion cycles
        const int STRIDE = 7;   // stride for deletion pattern (was 5)
        const int MIN_CHILD = 8;
        const int MAX_CHILD = 16;

        hilbert::RTree<int> tree(MIN_CHILD, MAX_CHILD, 2, 64);

        std::deque<int> values(N);
        std::deque<Rectangle> rects;
        // rects.reserve(N);

        // Create deterministic grid layout
        int grid = std::ceil(std::sqrt(N));
        for (int i = 0; i < N; i++) {
            values[i] = i;
            ll x = 5 * (i % grid);
            ll y = 5 * (i / grid);

            rects.push_back(makeRect({x, y}, {x + 5, y + 5}));
            tree.insert(rects[i], &values[i]);
        }

        bool all_passed = true;

        for (int cycle = 0; cycle < CYCLES; cycle++) {
            std::deque<int> deleted;
            // deleted.reserve(N / STRIDE);

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
            std::deque<int*> results =
                tree.search(makeRect({-1000, -1000}, {grid * 10, grid * 10}));

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

        hilbert::RTree<int> tree(MIN_CHILD, MAX_CHILD, 2, 64);
        std::deque<int> values(N);
        std::deque<Rectangle> rects;
        // rects.reserve(N);

        // Insert N rectangles in a grid
        int grid = std::ceil(std::sqrt(N));
        for (int i = 0; i < N; i++) {
            values[i] = i;
            ll x = (i % grid) * 1.5;
            ll y = (i / grid) * 1.5;

            rects.push_back(makeRect({x, y}, {x + 1, y + 1}));
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
        std::deque<int*> results = tree.search(makeRect({-1000, -1000}, {1000, 1000}));

        REQUIRE(results.size() == N);
    }
}
