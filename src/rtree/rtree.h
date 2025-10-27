#include <initializer_list>
#include <vector>

template <typename T>
class RTree {
   public:
    RTree();

    template <typename Itr>
    RTree(Itr, Itr);

    RTree(std::initializer_list<T>);
    RTree(const RTree<T>&);
    RTree(RTree<T>&&);
    RTree<T>& operator=(const RTree<T>&);
    RTree<T>& operator=(RTree<T>&&);
    ~RTree();

    void swap(RTree<T>&);

    void insert(const T&);

    template <typename Itr>
    void insert(Itr, Itr);

    void remove(const T&);
    bool search(const float* minBounds, const float* maxBounds, std::vector<int>& results) const;
};