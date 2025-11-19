#pragma once
#include <stdexcept>
#include <vector>

using ll = long long;

struct Range {
    ll start;
    ll end;
    Range(ll start, ll end) : start(start), end(end) {
        if (start > end)
            throw std::invalid_argument("Range end can't be less than range start");
    }
};

class Ranges {
    std::vector<Range> data;
    int capacity = 0;

   public:
    Ranges(int capacity) : capacity(capacity) {
        if (capacity > 0)
            data.reserve(capacity);
    }

    void add(const Range& r) {
        if (capacity && data.size() >= capacity)
            throw std::runtime_error("Range capacity exceeded");
        data.push_back(r);
    }

    int size() const { return data.size(); }

    std::vector<Range>::const_iterator begin() const { return data.begin(); }
    std::vector<Range>::const_iterator end() const { return data.end(); }
};
