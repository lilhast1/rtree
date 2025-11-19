#pragma once
#include <vector>

#include "ranges.h"

using ll = long long;
using Point = std::vector<ll>;

class HilbertCurve {
    int bits;
    int dim;
    ll len;

   public:
    HilbertCurve(int bits, int dim) : dim(dim), bits(bits), len(bits * dim) {
        if (bits < 1 || dim < 1)
            throw std::domain_error("You can't have negative dimensions or bits");
    }

    ll get_bits() const { return bits; }
    ll get_dim() const { return dim; }
    ll get_length() const { return len; }

    ll index(const Point& point) const;

    Point point(ll index) const;

    void point(ll index, Point& x) const;

    void transpose(ll indes, Point& x) const;

    Point transpose(ll index) const;

    static Point transposed_index(int bits, Point& x);

    static Point transposed_index_to_point(int bits, Point& x);

    ll to_index(Point& transposed_index) const;

    ll max_ordinate() const;

    ll max_index() const;

    Ranges query(const Point& a, const Point& b, int max_ranges, int buffer_size = 1024) const;
};