#pragma once
#include <functional>
#include <stdexcept>
#include <vector>
using ll = long long;
using Point = std::vector<ll>;

class Box
{
    Point lo;
    Point hi;
    int dim;

   public:
    Box(const Point& a, const Point& b) : lo(a), hi(b), dim(a.size())
    {
        if (lo.size() != hi.size())
            throw std::invalid_argument("Dimension mismatched for Box construction");
    }

    bool contains(const Point& p) const
    {
        for (int i = 0; i < dim; i++)
            if (p[i] < lo[i] || p[i] > hi[i])
                return false;
        return true;
    }

    void visit_perimiter(std::function<void(const Point&)> func) const
    {
        Point p(dim);

        std::function<void(int)> dfs = [&](int d)
        {
            if (d == dim)
            {
                bool isPerimiter = false;
                for (int i = 0; i < dim; i++)
                {
                    if (p[i] == lo[i] || p[i] == hi[i])
                    {
                        isPerimiter = true;
                        break;
                    }
                }
                if (isPerimiter)
                    func(p);
            }
            else
            {
                for (auto x = lo[d]; x <= hi[d]; x++)
                {
                    p[d] = x;
                    dfs(d + 1);
                }
            }
        };
        dfs(0);
    }
};