#pragma once
#include <vector>

#include "ranges.h"

using ll = long long;
using Point = std::vector<ll>;

/**
 * @class HilbertCurve
 * @brief Implements a d-dimensional Hilbert curve for mapping between points and Hilbert indices.
 *
 * This class provides functions to convert between d-dimensional points and
 * their corresponding Hilbert curve index, as well as transposition operations
 * used in Hilbert indexing.
 */
class HilbertCurve {
    int bits;  ///< Number of bits per dimension
    int dim;   ///< Number of dimensions
    ll len;    ///< Total number of bits = bits * dim

   public:
    /**
     * @brief Construct a Hilbert curve with specified bits and dimensions.
     * @param bits number of bits per dimension
     * @param dim number of dimensions
     * @throws std::domain_error if bits or dimensions are less than 1
     */
    HilbertCurve(int bits, int dim) : dim(dim), bits(bits), len(bits * dim) {
        if (bits < 1 || dim < 1)
            throw std::domain_error("You can't have negative dimensions or bits");
    }
    /** @brief Get the number of bits per dimension */
    [[nodiscard]] ll get_bits() const { return bits; }

    /** @brief Get the number of dimensions */
    [[nodiscard]] ll get_dim() const { return dim; }
    /** @brief Get the total number of bits */
    [[nodiscard]] ll get_length() const { return len; }
    /**
     * @brief Convert a point to its Hilbert curve index.
     * @param point d-dimensional point
     * @return Hilbert index of the point
     */
    ll index(const Point& point) const;
    /**
     * @brief Convert a Hilbert index to its corresponding point.
     * @param index Hilbert index
     * @return d-dimensional point
     */
    Point point(ll index) const;
    /**
     * @brief Convert a Hilbert index to a point (output parameter version).
     * @param index Hilbert index
     * @param x output point
     */
    void point(ll index, Point& x) const;
    /**
     * @brief Compute the transposed representation of a Hilbert index.
     * @param index Hilbert index
     * @param x output transposed point
     */
    void transpose(ll indes, Point& x) const;
    /**
     * @brief Compute the transposed representation of a Hilbert index.
     * @param index Hilbert index
     * @return transposed point
     */
    Point transpose(ll index) const;
    /**
     * @brief Static: convert a point to a transposed index (vector of integers).
     * @param bits number of bits per dimension
     * @param x point
     * @return transposed index
     */
    static Point transposed_index(int bits, Point& x);
    /**
     * @brief Static: convert a transposed index back to a point.
     * @param bits number of bits per dimension
     * @param x transposed index
     * @return d-dimensional point
     */
    static Point transposed_index_to_point(int bits, Point& x);

    /**
     * @brief Convert a transposed index to a Hilbert index.
     * @param transposed_index vector representing transposed index
     * @return Hilbert index
     */
    ll to_index(Point& transposed_index) const;
    /** @brief Maximum ordinate (coordinate value) allowed for this curve */
    [[nodiscard]] ll max_ordinate() const;
    /** @brief Maximum Hilbert index value for this curve */
    [[nodiscard]] ll max_index() const;

    /**
     * @brief Query the Hilbert curve for a hyper-rectangular range.
     * @param a lower corner of the range
     * @param b upper corner of the range
     * @param max_ranges maximum number of ranges to return
     * @param buffer_size optional buffer size for internal computation
     * @return Ranges object containing the Hilbert indices covering the query range
     */
    [[nodiscard]] Ranges query(const Point& a, const Point& b, int max_ranges,
                               int buffer_size = 1024) const;
};