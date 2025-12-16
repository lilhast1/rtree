#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "rtree/rtree.h"  // Adjust include path for your Gutman::RTree
#include "rtree_hilbert/hilbert.h"

// Helper alias (assuming you have makeRect defined somewhere)
//using Rectangle = Gutman::RTree<int>::Rectangle;

// Helper function to create a rectangle
using Rectangle = Gutman::Rectangle;

// Helper function to create a rectangle
Rectangle makeRect(std::vector<double> min, std::vector<double> max) {
    return Rectangle(min, max);
}


//using Rectangle = decltype(makeRect(std::vector<double>{}, std::vector<double>{}));

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
        rects.push_back(makeRect({(double)i, (double)i}, {(double)i+1, (double)i+1}));
        tree.insert(rects[i], &values[i]);
    }

    std::vector<int*> results = tree.search(makeRect({0.0,0.0},{10.0,10.0}));
    REQUIRE(results.size() == 5);
}

SECTION("Insert overlapping rectangles") {
    Gutman::RTree<int> tree(2,4);
    int val1=10, val2=20, val3=30;
    auto rect1 = makeRect({0.0,0.0},{5.0,5.0});
    auto rect2 = makeRect({3.0,3.0},{8.0,8.0});
    auto rect3 = makeRect({4.0,4.0},{6.0,6.0});
    tree.insert(rect1,&val1);
    tree.insert(rect2,&val2);
    tree.insert(rect3,&val3);

    std::vector<int*> results = tree.search(makeRect({4.0,4.0},{5.0,5.0}));
    REQUIRE(results.size() == 3);
}

SECTION("Insert triggering node split") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int> values(10);
    for(int i=0;i<10;i++){
        values[i]=i;
        auto rect = makeRect({(double)i,(double)i},{(double)i+0.5,(double)i+0.5});
        tree.insert(rect,&values[i]);
    }
    std::vector<int*> results = tree.search(makeRect({-1.0,-1.0},{20.0,20.0}));
    REQUIRE(results.size()==10);
}

SECTION("Insert identical rectangles with different values") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int> values = {1,2,3,4,5};
    auto rect = makeRect({5.0,5.0},{10.0,10.0});
    for(auto &v : values) tree.insert(rect,&v);

    std::vector<int*> results = tree.search(rect);
    REQUIRE(results.size()==5);
}

SECTION("Insert and search large dataset") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int> values(100);
    for(int i=0;i<100;i++){
        values[i]=i;
        double x=(i%10)*2.0;
        double y=(i/10)*2.0;
        tree.insert(makeRect({x,y},{x+1.5,y+1.5}), &values[i]);
    }
    std::vector<int*> results = tree.search(makeRect({-1.0,-1.0},{30.0,30.0}));
    REQUIRE(results.size()==100);
}


}

// ------------------- Search Tests -------------------
TEST_CASE("RTree search tests", "[search]") {
SECTION("Search empty tree") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int*> results = tree.search(makeRect({0,0},{10,10}));
    REQUIRE(results.size() == 0);
}


SECTION("Search no overlap") {
    Gutman::RTree<int> tree(2,4);
    int val=42;
    tree.insert(makeRect({0,0},{1,1}), &val);
    std::vector<int*> results = tree.search(makeRect({10,10},{20,20}));
    REQUIRE(results.size()==0);
}

SECTION("Search partial overlap") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int> values = {1,2,3,4,5};
    tree.insert(makeRect({0,0},{2,2}), &values[0]);
    tree.insert(makeRect({5,5},{7,7}), &values[1]);
    tree.insert(makeRect({10,10},{12,12}), &values[2]);
    tree.insert(makeRect({1,1},{3,3}), &values[3]);
    tree.insert(makeRect({8,8},{9,9}), &values[4]);

    std::vector<int*> results = tree.search(makeRect({0,0},{6,6}));
    REQUIRE(results.size()==3);
}

SECTION("Point query") {
    Gutman::RTree<int> tree(2,4);
    int val=99;
    tree.insert(makeRect({5,5},{10,10}), &val);
    std::vector<int*> results = tree.search(makeRect({7,7},{7,7}));
    REQUIRE(results.size()==1);
    REQUIRE(*results[0]==99);
}

SECTION("Search with exact boundaries") {
    Gutman::RTree<int> tree(2,4);
    int val1=10, val2=20, val3=30;
    tree.insert(makeRect({0,0},{5,5}), &val1);
    tree.insert(makeRect({5,5},{10,10}), &val2);
    tree.insert(makeRect({10,10},{15,15}), &val3);

    std::vector<int*> results = tree.search(makeRect({0,0},{5,5}));
    REQUIRE(results.size() >= 1);
}


}

// ------------------- Deletion Tests -------------------
TEST_CASE("RTree deletion tests", "[deletion]") {
SECTION("Delete single element") {
    Gutman::RTree<int> tree(2,4);
    int val=42;
    auto rect=makeRect({0,0},{1,1});
    tree.insert(rect,&val);
    tree.remove(rect);


    std::vector<int*> results = tree.search(rect);
    REQUIRE(results.size()==0);
}

SECTION("Delete from multiple elements") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int> values={1,2,3,4,5};
    std::vector<Rectangle> rects;
    for(size_t i=0;i<values.size();i++){
        rects.push_back(makeRect({(double)i,(double)i},{(double)i+1,(double)i+1}));
        tree.insert(rects[i], &values[i]);
    }
    tree.remove(rects[2]);
    std::vector<int*> results = tree.search(makeRect({0,0},{10,10}));
    REQUIRE(results.size()==4);
}

SECTION("Delete non-existent element") {
    Gutman::RTree<int> tree(2,4);
    int val=42;
    auto rect1=makeRect({0,0},{1,1});
    auto rect2=makeRect({10,10},{11,11});
    tree.insert(rect1,&val);
    tree.remove(rect2);

    std::vector<int*> results = tree.search(rect1);
    REQUIRE(results.size()==1);
}

SECTION("Delete and reinsert") {
    Gutman::RTree<int> tree(2,4);
    int val1=10, val2=20;
    auto rect=makeRect({0,0},{5,5});
    tree.insert(rect,&val1);
    tree.remove(rect);
    tree.insert(rect,&val2);

    std::vector<int*> results = tree.search(rect);
    REQUIRE(results.size()==1);
    REQUIRE(*results[0]==20);
}

SECTION("Delete multiple sequential elements") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int> values={1,2,3,4,5,6,7,8};
    std::vector<Rectangle> rects;
    for(size_t i=0;i<values.size();i++){
        rects.push_back(makeRect({(double)i,(double)i},{(double)i+1,(double)i+1}));
        tree.insert(rects[i], &values[i]);
    }
    tree.remove(rects[1]);
    tree.remove(rects[3]);
    tree.remove(rects[5]);

    std::vector<int*> results = tree.search(makeRect({-1,-1},{20,20}));
    REQUIRE(results.size()==5);
}

SECTION("Delete every other element") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int> values(20);
    std::vector<Rectangle> rects;
    for(int i=0;i<20;i++){
        values[i]=i;
        rects.push_back(makeRect({(double)i,(double)i},{(double)i+0.8,(double)i+0.8}));
        tree.insert(rects[i],&values[i]);
    }
    for(int i=0;i<20;i+=2) tree.remove(rects[i]);

    std::vector<int*> results = tree.search(makeRect({-1,-1},{25,25}));
    REQUIRE(results.size()==10);
}

SECTION("Delete from single element tree and reinsert") {
    Gutman::RTree<int> tree(2,4);
    int val=42;
    auto rect1=makeRect({0,0},{1,1});
    tree.insert(rect1,&val);
    tree.remove(rect1);

    int val2=99;
    auto rect2=makeRect({5,5},{6,6});
    tree.insert(rect2,&val2);

    std::vector<int*> results = tree.search(rect2);
    REQUIRE(results.size()==1);
    REQUIRE(*results[0]==99);
}


}

// ------------------- Edge Cases -------------------
TEST_CASE("RTree edge cases", "[edge]") {
SECTION("3D rectangles") {
    Gutman::RTree<int> tree(2,4);
    int val=42;
    auto rect = makeRect({0,0,0},{1,1,1});
    tree.insert(rect,&val);
    std::vector<int*> results = tree.search(rect);
    REQUIRE(results.size()==1);
}


SECTION("High dimensional 5D") {
    Gutman::RTree<int> tree(2,4);
    int val=42;
    auto rect = makeRect({0,0,0,0,0},{1,1,1,1,1});
    tree.insert(rect,&val);
    std::vector<int*> results = tree.search(rect);
    REQUIRE(results.size()==1);
}

SECTION("Zero area rectangle") {
    Gutman::RTree<int> tree(2,4);
    int val=42;
    auto rect = makeRect({5,5},{5,5});
    tree.insert(rect,&val);
    std::vector<int*> results = tree.search(rect);
    REQUIRE(results.size()==1);
}


}

// ------------------- Stress / Condense Tests -------------------
TEST_CASE("RTree stress and condense tests", "[stress]") {
    SECTION("Mixed insert/delete operations") {
    Gutman::RTree<int> tree(2,4);
    std::vector<int> values(15);
    std::vector<Rectangle> rects;
    for(int i=0;i<5;i++){
        values[i]=i;
        rects.push_back(makeRect({(double)i,(double)i},{(double)i+1,(double)i+1}));
        tree.insert(rects[i],&values[i]);
    }
    tree.remove(rects[1]);
    tree.remove(rects[3]);
    for(int i=5;i<10;i++){
        values[i]=i;
        rects.push_back(makeRect({(double)i,(double)i},{(double)i+1,(double)i+1}));
        tree.insert(rects[i],&values[i]);
    }
    tree.remove(rects[2]);
    tree.remove(rects[6]);
    tree.remove(rects[8]);
    for(int i=10;i<15;i++){
        values[i]=i;
        rects.push_back(makeRect({(double)i,(double)i},{(double)i+1,(double)i+1}));
        tree.insert(rects[i],&values[i]);
    }


    std::vector<int*> results = tree.search(makeRect({-1,-1},{20,20}));
    REQUIRE(results.size()==10);
    }

    SECTION("Stress test with millions of inserts and multiple splits") {
        Gutman::RTree<int> tree(2, 4);

        const size_t N = 1'000'000;   // 1 million
        std::vector<int> values(N);
        values.reserve(N);

        for (size_t i = 0; i < N; i++) {
            values[i] = static_cast<int>(i);

            // spread rectangles in a large grid,
            // similar logic to your original code
            double base_x = (i / 1000) * 3.0;   // 1000 per row
            double base_y = (i % 1000) * 3.0;

            tree.insert(
                makeRect({base_x, base_y}, {base_x + 2, base_y + 2}),
                &values[i]
            );
        }

        // Search bounding region
        auto results = tree.search(makeRect({-5, -5}, {3'500'000, 3'500'000}));

        // Search small cluster
        auto cluster_results = tree.search(makeRect({0, 0}, {5, 5}));

        REQUIRE(results.size() == N);
        REQUIRE(cluster_results.size() > 0);
    }

    SECTION("Deep tree with condense") {
        Gutman::RTree<int> tree(2,4);
        std::vector<int> values(100);
        std::vector<Rectangle> rects;
        for(int i=0;i<100;i++){
            values[i]=i;
            double cluster_x=(i/25)*10.0;
            double cluster_y=(i%25)*0.5;
            double x=cluster_x+(i%5)*0.1;
            double y=cluster_y;
            rects.push_back(makeRect({x,y},{x+0.05,y+0.05}));
            tree.insert(rects[i], &values[i]);
        }
        std::vector<int> to_delete={0,1,2,3,4,25,26,27,28,29,50,51,52,53,54,75,76,77,78,79};
        for(int idx:to_delete) tree.remove(rects[idx]);

        std::vector<int*> results = tree.search(makeRect({-10,-10},{50,50}));
        REQUIRE(results.size()==80);
    }

    SECTION("Extreme condense") {
        Gutman::RTree<int> tree(2,4);
        std::vector<int> values(200);
        std::vector<Rectangle> rects;
        for(int i=0;i<200;i++){
            values[i]=i;
            int cluster=i/20;
            int within_cluster=i%20;
            double x=cluster*5.0+(within_cluster%4)*0.1;
            double y=cluster*5.0+(within_cluster/4)*0.1;
            rects.push_back(makeRect({x,y},{x+0.05,y+0.05}));
            tree.insert(rects[i], &values[i]);
        }
        for(int cluster=0;cluster<10;cluster+=2)
            for(int j=0;j<20;j++)
                tree.remove(rects[cluster*20+j]);

        std::vector<int*> results = tree.search(makeRect({-5,-5},{60,60}));
        REQUIRE(results.size()==100);
    }

    SECTION("Sequential delete and reinsert - STRESS TEST") {
        const int N = 50000;          // number of rectangles
        const int CYCLES = 20;        // number of deletion/reinsertion cycles
        const int STRIDE = 7;         // stride for deletion pattern (was 5)
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
            std::vector<int*> results = tree.search(
                makeRect({-1000, -1000}, 
                        {(double)grid * 2.0, (double)grid * 2.0}
                    )
            );

            if (results.size() != N)
                all_passed = false;
        }

        REQUIRE(all_passed);
    }

    SECTION("Massive deletions with reinsertion - STRESS TEST") {
        const int N = 50000;       // total inserts
        const int DELETE_N = 30000; // how many to delete
        const int MIN_CHILD = 8;   // make tree fuller for stress
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


