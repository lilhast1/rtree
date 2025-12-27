#pragma once
#include <stdexcept>
#include <vector>

using ll = long long;

/**
 * @struct Range
 * @brief Represents a one-dimensional closed interval [start, end].
 *
 * Ensures that the start is not greater than the end.
 */
struct Range {
    ll start;  ///< Start of the range
    ll end;    ///< End of the range
    /**
     * @brief Construct a Range with start and end.
     * @param start range start
     * @param end range end
     * @throws std::invalid_argument if start > end
     */
    Range(ll start, ll end) : start(start), end(end) {
        if (start > end)
            throw std::invalid_argument("Range end can't be less than range start");
    }
};

/**
 * @class Ranges
 * @brief Manages a collection of Range objects with an optional capacity limit.
 *
 * Provides functionality to add ranges, iterate over them, and query the number
 * of ranges stored.
 */
class Ranges {
    std::vector<Range> data;  ///< Internal storage of ranges
    int capacity = 0;         ///< Maximum number of ranges allowed (0 = unlimited)

   public:
    /**
     * @brief Construct a Ranges container with optional capacity.
     * @param capacity Maximum number of ranges to store (0 for unlimited)
     */
    Ranges(int capacity) : capacity(capacity) {
        if (capacity > 0)
            data.reserve(capacity);
    }
    /**
     * @brief Add a range to the container.
     * @param r Range to add
     * @throws std::runtime_error if adding exceeds capacity
     */
    void add(const Range& r) {
        if (capacity && data.size() >= capacity)
            throw std::runtime_error("Range capacity exceeded");
        data.push_back(r);
    }
    /**
     * @brief Get the number of ranges stored.
     * @return number of ranges
     */
    int size() const { return data.size(); }
    /**
     * @brief Get iterator to the beginning of the ranges.
     * @return const iterator to first range
     */
    std::vector<Range>::const_iterator begin() const { return data.begin(); }
    /**
     * @brief Get iterator to the end of the ranges.
     * @return const iterator past the last range
     */
    std::vector<Range>::const_iterator end() const { return data.end(); }
};
