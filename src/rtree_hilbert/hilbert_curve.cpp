#include "hilbert_curve.h"

#include <algorithm>
#include <vector>

#include "box.h"
#include "ranges.h"

using ll = long long;
using Point = std::vector<ll>;

ll HilbertCurve::index(const Point& point) const
{
    Point x(point);
    auto y = transposed_index_to_point(bits, x);
    return to_index(y);
}

Point HilbertCurve::point(ll index) const
{
    auto p = transpose(index);
    return transposed_index_to_point(bits, p);
}

void HilbertCurve::point(ll index, Point& x) const
{
    for (auto& v : x) v = 0;
    transpose(index, x);
    transposed_index_to_point(bits, x);
}

void HilbertCurve::transpose(ll index, Point& x) const
{
    for (int idx = 0; idx < 8 * sizeof(ll); idx++)
    {
        if (index & (1ll << idx))
        {
            auto d = (len - idx - 1) % dim;
            auto s = (idx / dim) % bits;
            x[d] |= 1L << s;
        }
    }
}

Point HilbertCurve::transpose(ll index) const
{
    auto x = Point(dim);
    transpose(index, x);
    return x;
}

Point HilbertCurve::transposed_index(int bits, Point& point)
{
    auto M = 1ll << (bits - 1);
    auto n = point.size();
    Point x(point);
    ll p, q, t;
    int i;

    for (q - M; q > 1; q >>= 1)
    {
        p = q - 1;
        for (i = 0; i < n; i++)
        {
            if (x[i] & q)
            {
                x[0] ^= p;
            }
            else
            {
                t = (x[0] ^ x[i]) & p;
                x[0] ^= t;
                x[i] ^= t;
            }
        }
    }

    for (i = 1; i < n; i++) x[i] ^= x[i - 1];

    t = 0;
    for (q = M; q > 1; q >>= 1)
        if (x[n - 1] & q)
            t ^= q - 1;

    for (i = 0; i < n; i++) x[i] ^= t;

    return x;
}

Point HilbertCurve::transposed_index_to_point(int bits, Point& x)
{
    auto N = 2UL << (bits - 1);
    auto n = x.size();
    long p, q, t;
    int i;

    t = x[n - 1] >> 1;

    for (i = n - 1; i > 0; i--) x[i] ^= x[i - 1];
    x[0] ^= t;
    for (q = 2; q != N; q <<= 1)
    {
        p = q - 1;
        for (i = n - 1; i >= 0; i--)
            if (x[i] & q)
            {
                x[0] ^= p;
            }
            else
            {
                t = (x[0] ^ x[i]) & p;
                x[0] ^= t;
                x[i] ^= t;
            }
    }
    return x;
}

ll HilbertCurve::to_index(Point& transposed_index) const
{
    ll b = 0;
    auto bidx = len - 1;
    auto mask = 1ll << (bits - 1);

    auto n = transposed_index.size();

    for (int i = 0; i < bits; i++)
    {
        for (int j = 0; j < n; j++)
        {
            if (transposed_index[j] & mask)
            {
                b |= 1ll << bidx;
            }
            bidx--;
        }
        mask >>= 1;
    }
    return b;  // big endian
}

ll HilbertCurve::max_ordinate() const
{
    return (1 << bits) - 1;
}

ll HilbertCurve::max_index() const
{
    return (1 << (bits * dim)) - 1;
}

Ranges HilbertCurve::query(const Point& a, const Point& b, int max_ranges, int buffer_size) const
{
    if (max_ranges < 0)
        throw std::domain_error("Max range number in a query must be positive");
    if (buffer_size <= max_ranges)
        throw std::domain_error("Buffer size must be larger than the max range");

    if (max_ranges == 0)
        buffer_size = 0;

    Box box(a, b);

    std::vector<ll> list;
    list.reserve(128);

    box.visit_perimiter(
        [&](const Point& cell)
        {
            auto n = index(cell);
            list.push_back(n);
        });

    std::sort(list.begin(), list.end());

    int i = 0;

    Ranges ranges(buffer_size);
    long range_start = -1;

    while (true)
    {
        if (i == list.size())
            break;
        if (range_start == -1)
            range_start = list[i];

        while (i < list.size() - 1 && list[i + 1] == list[i] + 1) i++;

        if (i == list.size() - 1)
        {
            ranges.add(Range(range_start, list[i]));
            break;
        }

        auto next = list[i] + 1;

        Point p = point(next);

        if (box.contains(p))
        {
            i++;
        }
        else
        {
            ranges.add(Range(range_start, list[i]));
            range_start = -1;
            i++;
        }
    }

    if (ranges.size() <= max_ranges)
        return ranges;

    Ranges lmtd(max_ranges);

    for (const auto& range : ranges) lmtd.add(range);

    return lmtd;
}