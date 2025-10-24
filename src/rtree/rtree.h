#include <vector>

class RTree {
   public:
    RTree();
    ~RTree();

    void insert(int id, const float* minBounds, const float* maxBounds);
    void remove(int id, const float* minBounds, const float* maxBounds);
    bool search(const float* minBounds, const float* maxBounds, std::vector<int>& results) const;
};