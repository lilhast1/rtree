#pragma once
#include <functional>
#include <stdexcept>
#include <vector>
using ll = long long;
using Point = std::vector<ll>;
/**
 * @class Box
 * @brief Represents an axis-aligned hyper-rectangular box in N-dimensional space.
 *
 * Provides functionality to check whether a point is inside the box
 * and to iterate over all points on the perimeter.
 */
class Box {
    Point lo;  ///< Lower corner of the box (inclusive)
    Point hi;  ///< Higher corner of the box (inclusive)
    int dim;   ///< Dimension of the box

   public:
    /**
     * @brief Construct a Box from two corners.
     * @param a Lower corner coordinates
     * @param b Upper corner coordinates
     * @throws std::invalid_argument if the dimensions of `a` and `b` mismatch
     */
    Box(const Point& a, const Point& b) : lo(a), hi(b), dim(a.size()) {
        if (lo.size() != hi.size())
            throw std::invalid_argument("Dimension mismatched for Box construction");
    }
    /**
     * @brief Check if a point is contained within the box.
     * @param p The point to check
     * @return true if the point is inside the box (inclusive), false otherwise
     */
    bool contains(const Point& p) const {
        for (int i = 0; i < dim; i++)
            if (p[i] < lo[i] || p[i] > hi[i])
                return false;
        return true;
    }
    /**
     * @brief Visit all points on the perimeter of the box.
     *
     * Calls the provided function for each point lying on the box's perimeter.
     * The perimeter includes any point where at least one coordinate
     * is equal to the lower or higher bound of the box.
     *
     * @param func Function to call for each perimeter point
     */
    void visit_perimiter(std::function<void(const Point&)> func) const {
        Point p(dim);

        std::function<void(int)> dfs = [&](int d) {
            if (d == dim) {
                bool isPerimiter = false;
                for (int i = 0; i < dim; i++) {
                    if (p[i] == lo[i] || p[i] == hi[i]) {
                        isPerimiter = true;
                        break;
                    }
                }
                if (isPerimiter)
                    func(p);
            } else {
                for (auto x = lo[d]; x <= hi[d]; x++) {
                    p[d] = x;
                    dfs(d + 1);
                }
            }
        };
        dfs(0);
    }
};