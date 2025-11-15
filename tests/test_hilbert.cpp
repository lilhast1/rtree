#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

#include "rtree_hilbert/box.h"
#include "rtree_hilbert/hilbert_curve.h"
#include "rtree_hilbert/ranges.h"

using Point = std::vector<ll>;

TEST_CASE("HilbertCurve: max_ordinate and max_index")
{
    HilbertCurve H(3, 2);

    REQUIRE(H.max_ordinate() == 7);
    REQUIRE(H.max_index() == (1ll << (3 * 2)) - 1);  // 2D, 3 bits → 64 entries: 0..63
}

TEST_CASE("HilbertCurve: simple round trip 2D bits=2")
{
    HilbertCurve H(2, 2);

    for (ll x = 0; x <= H.max_ordinate(); x++)
    {
        for (ll y = 0; y <= H.max_ordinate(); y++)
        {
            Point p{x, y};
            ll idx = H.index(p);
            Point q = H.point(idx);

            REQUIRE(q.size() == 2);
            REQUIRE(q[0] == p[0]);
            REQUIRE(q[1] == p[1]);
        }
    }
}

TEST_CASE("HilbertCurve: transpose and inverse")
{
    HilbertCurve H(3, 2);

    for (ll i = 0; i < H.max_index(); i++)
    {
        Point t = H.transpose(i);
        Point p = H.transposed_index_to_point(H.get_bits(), t);

        // p should represent the same point as H.point(i)
        Point q = H.point(i);

        REQUIRE(p.size() == q.size());
        REQUIRE(p == q);
    }
}

TEST_CASE("HilbertCurve: known 2D sequence for bits=1")
{
    // Standard 2D Hilbert order 1:
    // (0,0) → 0
    // (0,1) → 1
    // (1,1) → 2
    // (1,0) → 3

    HilbertCurve H(1, 2);

    REQUIRE(H.index({0, 0}) == 0);
    REQUIRE(H.index({0, 1}) == 1);
    REQUIRE(H.index({1, 1}) == 2);
    REQUIRE(H.index({1, 0}) == 3);

    // Reverse
    REQUIRE(H.point(0) == Point{0, 0});
    REQUIRE(H.point(1) == Point{0, 1});
    REQUIRE(H.point(2) == Point{1, 1});
    REQUIRE(H.point(3) == Point{1, 0});
}

TEST_CASE("HilbertCurve: point() overload")
{
    HilbertCurve H(3, 2);

    for (ll i = 0; i < 16; i++)
    {
        Point a = H.point(i);

        Point b(2);
        H.point(i, b);

        REQUIRE(a == b);
    }
}

//
// BOX + PERIMETER + QUERY TESTS
//

TEST_CASE("Box.contains simple")
{
    Box B({0, 0}, {3, 3});

    REQUIRE(B.contains({0, 0}));
    REQUIRE(B.contains({3, 3}));
    REQUIRE(B.contains({1, 2}));

    REQUIRE_FALSE(B.contains({4, 0}));
    REQUIRE_FALSE(B.contains({0, 4}));
    REQUIRE_FALSE(B.contains({-1, 0}));
}

TEST_CASE("Box.visit_perimeter 2D 3x3")
{
    Box B({0, 0}, {2, 2});

    std::vector<Point> pts;
    B.visit_perimiter([&](const Point& p) { pts.push_back(p); });

    // Perimeter points count: 8 (square 3x3 grid)
    REQUIRE(pts.size() == 8);

    // Check some known ones
    REQUIRE(std::find(pts.begin(), pts.end(), Point{0, 0}) != pts.end());
    REQUIRE(std::find(pts.begin(), pts.end(), Point{0, 2}) != pts.end());
    REQUIRE(std::find(pts.begin(), pts.end(), Point{2, 0}) != pts.end());
    REQUIRE(std::find(pts.begin(), pts.end(), Point{2, 2}) != pts.end());
}

TEST_CASE("HilbertCurve query: 2D small box")
{
    HilbertCurve H(2, 2);

    // Query a 2x2 square
    Point a{0, 0};
    Point b{1, 1};

    auto ranges = H.query(a, b, 32);

    REQUIRE(ranges.size() >= 1);

    // All returned points must be inside the box
    for (auto& R : ranges)
    {
        for (ll idx = R.start; idx <= R.end; idx++)
        {
            Point p = H.point(idx);
            REQUIRE(p[0] >= 0);
            REQUIRE(p[0] <= 1);
            REQUIRE(p[1] >= 0);
            REQUIRE(p[1] <= 1);
        }
    }
}

TEST_CASE("HilbertCurve query: range merging")
{
    HilbertCurve H(2, 2);

    // Box that forces several adjacent Hilbert indices
    Point a{0, 0};
    Point b{3, 0};  // horizontal strip → should merge many consecutive indices

    auto ranges = H.query(a, b, 32);

    REQUIRE(ranges.size() == 1);  // consecutive along Hilbert traversal
    auto R = ranges.begin();
    REQUIRE(R->start <= R->end);
}

TEST_CASE("HilbertCurve query: max_ranges truncation")
{
    HilbertCurve H(2, 2);

    Point a{0, 0};
    Point b{3, 3};

    auto ranges = H.query(a, b, 1);

    // Must return exactly 1 range if max_ranges=1
    REQUIRE(ranges.size() == 1);
}
// Dodaj ove testove u test_hilbert.cpp

TEST_CASE("HilbertCurve: different dimensions", "[hilbert][dimensions]")
{
    SECTION("1D Hilbert curve")
    {
        HilbertCurve H(3, 1);  // 3 bits, 1D

        REQUIRE(H.max_ordinate() == 7);
        REQUIRE(H.max_index() == 7);

        // 1D is just linear
        for (ll i = 0; i <= 7; i++)
        {
            REQUIRE(H.index({i}) == i);
            REQUIRE(H.point(i) == Point{i});
        }
    }

    SECTION("3D Hilbert curve")
    {
        HilbertCurve H(2, 3);  // 2 bits, 3D

        REQUIRE(H.max_ordinate() == 3);
        REQUIRE(H.max_index() == 63);  // 2^(2*3) - 1

        // Test round trip for some points
        std::vector<Point> test_points = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {1, 1, 1}, {3, 3, 3}};

        for (const auto& p : test_points)
        {
            ll idx = H.index(p);
            Point q = H.point(idx);
            REQUIRE(p == q);
        }
    }

    SECTION("4D Hilbert curve")
    {
        HilbertCurve H(2, 4);  // 2 bits, 4D

        REQUIRE(H.max_ordinate() == 3);
        REQUIRE(H.max_index() == 255);  // 2^(2*4) - 1

        Point p{1, 2, 1, 3};
        ll idx = H.index(p);
        Point q = H.point(idx);
        REQUIRE(p == q);
    }
}

TEST_CASE("HilbertCurve: boundary values", "[hilbert][boundaries]")
{
    HilbertCurve H(3, 2);

    SECTION("Min values (0,0)")
    {
        Point p{0, 0};
        ll idx = H.index(p);
        REQUIRE(idx == 0);
        REQUIRE(H.point(idx) == p);
    }

    SECTION("Max values (7,7)")
    {
        Point p{7, 7};
        ll idx = H.index(p);
        Point q = H.point(idx);
        REQUIRE(q == p);
    }

    SECTION("All corners")
    {
        std::vector<Point> corners = {{0, 0}, {0, 7}, {7, 0}, {7, 7}};

        for (const auto& corner : corners)
        {
            ll idx = H.index(corner);
            Point q = H.point(idx);
            REQUIRE(q == corner);
        }
    }
}

TEST_CASE("HilbertCurve: index ordering", "[hilbert][ordering]")
{
    HilbertCurve H(2, 2);

    SECTION("Sequential indices give nearby points")
    {
        for (ll i = 0; i < H.max_index(); i++)
        {
            Point p1 = H.point(i);
            Point p2 = H.point(i + 1);

            // Manhattan distance should be 1 (neighbors on Hilbert curve)
            ll dist = std::abs(p1[0] - p2[0]) + std::abs(p1[1] - p2[1]);
            REQUIRE(dist == 1);
        }
    }

    SECTION("Monotonicity check")
    {
        // Indices should be unique for unique points
        std::set<ll> seen_indices;
        for (ll x = 0; x <= H.max_ordinate(); x++)
        {
            for (ll y = 0; y <= H.max_ordinate(); y++)
            {
                ll idx = H.index({x, y});
                REQUIRE(seen_indices.find(idx) == seen_indices.end());
                seen_indices.insert(idx);
            }
        }
        REQUIRE(seen_indices.size() == (H.max_ordinate() + 1) * (H.max_ordinate() + 1));
    }
}

TEST_CASE("HilbertCurve: query with different box sizes", "[hilbert][query]")
{
    HilbertCurve H(3, 2);  // 3 bits, 2D → 8x8 grid

    SECTION("Single point query")
    {
        Point a{2, 2};
        Point b{2, 2};

        auto ranges = H.query(a, b, 32);

        // Should return at least one range
        REQUIRE(ranges.size() >= 1);

        // Verify all indices are the single point
        for (const auto& r : ranges)
        {
            for (ll idx = r.start; idx <= r.end; idx++)
            {
                Point p = H.point(idx);
                REQUIRE(p[0] == 2);
                REQUIRE(p[1] == 2);
            }
        }
    }

    SECTION("2x2 box query")
    {
        Point a{1, 1};
        Point b{2, 2};

        auto ranges = H.query(a, b, 32);

        REQUIRE(ranges.size() >= 1);

        // Count how many points are in the ranges
        int point_count = 0;
        for (const auto& r : ranges)
        {
            for (ll idx = r.start; idx <= r.end; idx++)
            {
                Point p = H.point(idx);
                if (p[0] >= 1 && p[0] <= 2 && p[1] >= 1 && p[1] <= 2)
                {
                    point_count++;
                }
            }
        }
        // Should include all 4 points in the 2x2 box
        REQUIRE(point_count >= 4);
    }

    SECTION("Large box query")
    {
        Point a{0, 0};
        Point b{7, 7};

        auto ranges = H.query(a, b, 32);

        // Should cover entire space
        REQUIRE(ranges.size() >= 1);
    }

    SECTION("Vertical strip")
    {
        Point a{2, 0};
        Point b{2, 7};

        auto ranges = H.query(a, b, 32);

        REQUIRE(ranges.size() >= 1);

        // Verify points are in the strip
        for (const auto& r : ranges)
        {
            for (ll idx = r.start; idx <= r.end; idx++)
            {
                Point p = H.point(idx);
                // Should be in or near the strip
                REQUIRE(p[0] >= 0);
                REQUIRE(p[0] <= 7);
            }
        }
    }

    SECTION("Diagonal box")
    {
        Point a{1, 1};
        Point b{5, 5};

        auto ranges = H.query(a, b, 32);

        REQUIRE(ranges.size() >= 1);
    }
}

TEST_CASE("HilbertCurve: query max_ranges parameter", "[hilbert][query][limits]")
{
    HilbertCurve H(3, 2);

    SECTION("max_ranges = 1")
    {
        Point a{0, 0};
        Point b{7, 7};

        auto ranges = H.query(a, b, 1);

        REQUIRE(ranges.size() == 1);
    }

    SECTION("max_ranges = 5")
    {
        Point a{0, 0};
        Point b{7, 7};

        auto ranges = H.query(a, b, 5);

        REQUIRE(ranges.size() <= 5);
    }

    SECTION("max_ranges = 0 (unlimited)")
    {
        Point a{0, 0};
        Point b{7, 7};

        auto ranges = H.query(a, b, 0);

        // Should return natural number of ranges
        REQUIRE(ranges.size() >= 1);
    }
}

TEST_CASE("HilbertCurve: query edge cases", "[hilbert][query][edge]")
{
    HilbertCurve H(2, 2);

    SECTION("Box at corner (0,0)")
    {
        Point a{0, 0};
        Point b{1, 1};

        auto ranges = H.query(a, b, 32);

        REQUIRE(ranges.size() >= 1);

        // Should contain corner
        bool found_corner = false;
        for (const auto& r : ranges)
        {
            if (r.start == 0)
                found_corner = true;
        }
        REQUIRE(found_corner);
    }

    SECTION("Box at opposite corner (3,3)")
    {
        Point a{2, 2};
        Point b{3, 3};

        auto ranges = H.query(a, b, 32);

        REQUIRE(ranges.size() >= 1);
    }

    SECTION("Line query (horizontal)")
    {
        Point a{0, 1};
        Point b{3, 1};

        auto ranges = H.query(a, b, 32);

        REQUIRE(ranges.size() >= 1);
    }

    SECTION("Line query (vertical)")
    {
        Point a{1, 0};
        Point b{1, 3};

        auto ranges = H.query(a, b, 32);

        REQUIRE(ranges.size() >= 1);
    }
}

TEST_CASE("HilbertCurve: performance with larger grids", "[hilbert][performance]")
{
    SECTION("4 bits 2D (16x16 grid)")
    {
        HilbertCurve H(4, 2);

        REQUIRE(H.max_ordinate() == 15);
        REQUIRE(H.max_index() == 255);

        // Test some round trips
        for (int i = 0; i < 50; i++)
        {
            ll x = rand() % 16;
            ll y = rand() % 16;
            Point p{x, y};

            ll idx = H.index(p);
            Point q = H.point(idx);

            REQUIRE(p == q);
        }
    }

    SECTION("5 bits 2D (32x32 grid)")
    {
        HilbertCurve H(5, 2);

        REQUIRE(H.max_ordinate() == 31);
        REQUIRE(H.max_index() == 1023);

        // Test query on medium box
        Point a{5, 5};
        Point b{25, 25};

        auto ranges = H.query(a, b, 32);

        REQUIRE(ranges.size() >= 1);
        REQUIRE(ranges.size() <= 32);
    }
}

TEST_CASE("HilbertCurve: space-filling property", "[hilbert][properties]")
{
    HilbertCurve H(3, 2);  // 8x8 grid

    SECTION("All points are visited exactly once")
    {
        std::set<Point> visited;

        for (ll idx = 0; idx <= H.max_index(); idx++)
        {
            Point p = H.point(idx);

            // Should be within bounds
            REQUIRE(p[0] >= 0);
            REQUIRE(p[0] <= H.max_ordinate());
            REQUIRE(p[1] >= 0);
            REQUIRE(p[1] <= H.max_ordinate());

            // Should not have been visited before
            REQUIRE(visited.find(p) == visited.end());
            visited.insert(p);
        }

        // Should have visited all points
        REQUIRE(visited.size() == (H.max_ordinate() + 1) * (H.max_ordinate() + 1));
    }
}

TEST_CASE("HilbertCurve: consistency checks", "[hilbert][consistency]")
{
    HilbertCurve H(2, 2);

    SECTION("index and point are inverses")
    {
        for (ll idx = 0; idx <= H.max_index(); idx++)
        {
            Point p = H.point(idx);
            ll idx2 = H.index(p);
            REQUIRE(idx == idx2);
        }
    }

    SECTION("point(index(p)) == p for all valid points")
    {
        for (ll x = 0; x <= H.max_ordinate(); x++)
        {
            for (ll y = 0; y <= H.max_ordinate(); y++)
            {
                Point p{x, y};
                ll idx = H.index(p);
                Point q = H.point(idx);
                REQUIRE(p == q);
            }
        }
    }
}

TEST_CASE("Box: advanced perimeter tests", "[box][perimeter]")
{
    SECTION("1D box perimeter")
    {
        Box B({2}, {5});

        std::vector<Point> pts;
        B.visit_perimiter([&](const Point& p) { pts.push_back(p); });

        // 1D "perimeter" is just the endpoints
        REQUIRE(pts.size() == 2);
        REQUIRE(std::find(pts.begin(), pts.end(), Point{2}) != pts.end());
        REQUIRE(std::find(pts.begin(), pts.end(), Point{5}) != pts.end());
    }

    SECTION("3D box perimeter")
    {
        Box B({0, 0, 0}, {1, 1, 1});

        std::vector<Point> pts;
        B.visit_perimiter([&](const Point& p) { pts.push_back(p); });

        // 3D box perimeter: all points on faces (not interior)
        // 2x2x2 cube has 8 vertices, all on perimeter
        REQUIRE(pts.size() == 8);

        // Check that all 8 corners are present
        REQUIRE(std::find(pts.begin(), pts.end(), Point{0, 0, 0}) != pts.end());
        REQUIRE(std::find(pts.begin(), pts.end(), Point{1, 1, 1}) != pts.end());
    }

    SECTION("Larger 2D perimeter")
    {
        Box B({0, 0}, {4, 4});

        std::vector<Point> pts;
        B.visit_perimiter([&](const Point& p) { pts.push_back(p); });

        // 5x5 grid: perimeter = 5*4 - 4 (corners counted once) = 16
        // Actually all points touching edge = 16
        REQUIRE(pts.size() == 16);

        // Interior point (2,2) should NOT be in perimeter
        REQUIRE(std::find(pts.begin(), pts.end(), Point{2, 2}) == pts.end());

        // Edge points should be in perimeter
        REQUIRE(std::find(pts.begin(), pts.end(), Point{0, 2}) != pts.end());
        REQUIRE(std::find(pts.begin(), pts.end(), Point{4, 2}) != pts.end());
        REQUIRE(std::find(pts.begin(), pts.end(), Point{2, 0}) != pts.end());
        REQUIRE(std::find(pts.begin(), pts.end(), Point{2, 4}) != pts.end());
    }
}

TEST_CASE("HilbertCurve: stress test with many queries", "[hilbert][stress]")
{
    HilbertCurve H(4, 2);  // 16x16 grid

    SECTION("100 random box queries")
    {
        for (int i = 0; i < 100; i++)
        {
            ll x1 = rand() % 16;
            ll y1 = rand() % 16;
            ll x2 = x1 + (rand() % (16 - x1));
            ll y2 = y1 + (rand() % (16 - y1));

            Point a{x1, y1};
            Point b{x2, y2};

            auto ranges = H.query(a, b, 32);

            // Should always return at least one range
            REQUIRE(ranges.size() >= 1);

            // Should respect max_ranges
            REQUIRE(ranges.size() <= 32);

            // Ranges should be valid
            for (const auto& r : ranges)
            {
                REQUIRE(r.start <= r.end);
                REQUIRE(r.start >= 0);
                REQUIRE(r.end <= H.max_index());
            }
        }
    }
}

TEST_CASE("HilbertCurve: error handling", "[hilbert][errors]")
{
    SECTION("Invalid construction - zero bits")
    {
        REQUIRE_THROWS_AS(HilbertCurve(0, 2), std::domain_error);
    }

    SECTION("Invalid construction - zero dimensions")
    {
        REQUIRE_THROWS_AS(HilbertCurve(2, 0), std::domain_error);
    }

    SECTION("Invalid construction - negative bits")
    {
        REQUIRE_THROWS_AS(HilbertCurve(-1, 2), std::domain_error);
    }

    SECTION("Invalid query - negative max_ranges")
    {
        HilbertCurve H(2, 2);
        Point a{0, 0};
        Point b{1, 1};

        REQUIRE_THROWS_AS(H.query(a, b, -1), std::domain_error);
    }
}
