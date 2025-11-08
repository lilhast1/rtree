#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "rtree_hilbert/hilbert_curve.h"
#include "rtree_hilbert/box.h"
#include "rtree_hilbert/ranges.h"

using Point = std::vector<ll>;

TEST_CASE("HilbertCurve: max_ordinate and max_index") {
    HilbertCurve H(3,  2);

    REQUIRE(H.max_ordinate() == 7);
    REQUIRE(H.max_index() == (1ll << (3 * 2)) - 1); // 2D, 3 bits → 64 entries: 0..63
}

TEST_CASE("HilbertCurve: simple round trip 2D bits=2") {
    HilbertCurve H(2,  2);

    for (ll x = 0; x <= H.max_ordinate(); x++) {
        for (ll y = 0; y <= H.max_ordinate(); y++) {

            Point p{ x, y };
            ll idx = H.index(p);
            Point q = H.point(idx);

            REQUIRE(q.size() == 2);
            REQUIRE(q[0] == p[0]);
            REQUIRE(q[1] == p[1]);
        }
    }
}

TEST_CASE("HilbertCurve: transpose and inverse") {
    HilbertCurve H(3,  2);

    for (ll i = 0; i < H.max_index(); i++) {
        Point t = H.transpose(i);
        Point p = H.transposed_index_to_point(H.get_bits(), t);

        // p should represent the same point as H.point(i)
        Point q = H.point(i);

        REQUIRE(p.size() == q.size());
        REQUIRE(p == q);
    }
}

TEST_CASE("HilbertCurve: known 2D sequence for bits=1") {
    // Standard 2D Hilbert order 1:
    // (0,0) → 0
    // (0,1) → 1
    // (1,1) → 2
    // (1,0) → 3

    HilbertCurve H(1,  2);

    REQUIRE(H.index({0,0}) == 0);
    REQUIRE(H.index({0,1}) == 1);
    REQUIRE(H.index({1,1}) == 2);
    REQUIRE(H.index({1,0}) == 3);

    // Reverse
    REQUIRE(H.point(0) == Point{0,0});
    REQUIRE(H.point(1) == Point{0,1});
    REQUIRE(H.point(2) == Point{1,1});
    REQUIRE(H.point(3) == Point{1,0});
}

TEST_CASE("HilbertCurve: point() overload") {
    HilbertCurve H(3,  2);

    for (ll i = 0; i < 16; i++) {
        Point a = H.point(i);

        Point b(2);
        H.point(i, b);

        REQUIRE(a == b);
    }
}

//
// BOX + PERIMETER + QUERY TESTS
//

TEST_CASE("Box.contains simple") {
    Box B({0,0}, {3,3});

    REQUIRE(B.contains({0,0}));
    REQUIRE(B.contains({3,3}));
    REQUIRE(B.contains({1,2}));

    REQUIRE_FALSE(B.contains({4,0}));
    REQUIRE_FALSE(B.contains({0,4}));
    REQUIRE_FALSE(B.contains({-1,0}));
}

TEST_CASE("Box.visit_perimeter 2D 3x3") {
    Box B({0,0}, {2,2});

    std::vector<Point> pts;
    B.visit_perimiter([&](const Point& p){ pts.push_back(p); });

    // Perimeter points count: 8 (square 3x3 grid)
    REQUIRE(pts.size() == 8);

    // Check some known ones
    REQUIRE(std::find(pts.begin(), pts.end(), Point{0,0}) != pts.end());
    REQUIRE(std::find(pts.begin(), pts.end(), Point{0,2}) != pts.end());
    REQUIRE(std::find(pts.begin(), pts.end(), Point{2,0}) != pts.end());
    REQUIRE(std::find(pts.begin(), pts.end(), Point{2,2}) != pts.end());
}

TEST_CASE("HilbertCurve query: 2D small box") {
    HilbertCurve H(2,  2);

    // Query a 2x2 square
    Point a{0,0};
    Point b{1,1};

    auto ranges = H.query(a, b, 32);

    REQUIRE(ranges.size() >= 1);

    // All returned points must be inside the box
    for (auto& R : ranges) {
        for (ll idx = R.start; idx <= R.end; idx++) {
            Point p = H.point(idx);
            REQUIRE(p[0] >= 0);
            REQUIRE(p[0] <= 1);
            REQUIRE(p[1] >= 0);
            REQUIRE(p[1] <= 1);
        }
    }
}

TEST_CASE("HilbertCurve query: range merging") {
    HilbertCurve H(2,  2);

    // Box that forces several adjacent Hilbert indices
    Point a{0,0};
    Point b{3,0};   // horizontal strip → should merge many consecutive indices

    auto ranges = H.query(a, b, 32);

    REQUIRE(ranges.size() == 1); // consecutive along Hilbert traversal
    auto R = ranges.begin();
    REQUIRE(R->start <= R->end);
}

TEST_CASE("HilbertCurve query: max_ranges truncation") {
    HilbertCurve H(2,  2);

    Point a{0,0};
    Point b{3,3};

    auto ranges = H.query(a, b, 1);

    // Must return exactly 1 range if max_ranges=1
    REQUIRE(ranges.size() == 1);
}
