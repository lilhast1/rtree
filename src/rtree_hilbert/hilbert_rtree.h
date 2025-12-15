#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include
#include "rtree/rtree.h"
#include "rtree_hilbert/hilbert_curve.h"

using ll = long long;
using Point = std::vector<ll>;
using namespace std::literals::string_literals;

namespace hilbert {

const auto not_implemented = std::logic_error("Not implemented"s);

struct Rectangle {
    Point get_center() const;
    Point get_lower() const;
    Point get_upper() const;
    bool intersects(const Rectangle& rect) const;
    bool contains(const Rectangle& rect) const;
    bool operator==(const Rectangle& other) const;
};

template <typename T>
struct Node;

template <typename T>
struct NodeEntry {
    ll get_lhv();
    Rectangle& get_mbr();
    bool is_leaf();
};

template <typename T>
struct LeafEntry : NodeEntry<T> {};

template <typename T>
struct InnerNode : NodeEntry<T> {
    Node<T*> get_node();
};

template <typename T>
struct Node {
    Node(int min_entries, int max_entries, HilbertCurve& curve) { throw not_implemented; }
    bool overflow();
    bool underflow();
    Node<T>* get_prev_siblings();
    Node<T>* get_next_siblings();
    Node<T>* set_next_siblings();
    Node<T>* get_parent();
    Node<T>* set_parent();
    bool is_leaf();
    void set_leaf();
    ll get_lhv();
    Rectangle& get_mbr();
    void insert_leaf_entry(LeafEntry<T>* entry);
    void insert_inner_entry(InnerNode<T>* entry);
    void remove_leaf_entry(Node<T>* child);
    void remove_inner_entry(InnerNode<T>* entry);
    void adjust_mbr();
    void adjust_lhv();
    std::vector<Node<T>*> get_siblings(int num);
    std::vector<NodeEntry<T>*> get_entries();
    void reset_entries();
};

template <typename T>
class RTree {
   public:
    RTree() { throw not_implemented; }
    ~RTree() { throw not_implemented; }

    std::vector<T*> search(const Rectangle& search_rect) { throw not_implemented; }
    void insert(const Rectangle& rect, T* elem) { throw not_implemented; }
    void remove(const Rectangle& rect) { throw not_implemented; }

   private:
    Node<T*> choose_leaf(Node<T>* node, ll hilbert_value) { throw not_implemented; }
    void redistribute_entries(std::vector<NodeEntry<T>>& entries, std::vector<Node<T>*>& siblings) {
        throw not_implemented;
    }
    Node<T>* handle_overflow(Node<T>* node, NodeEntry<T>* entry, std::vector<Node<T>*>& siblings) {
        throw not_implemented;
    }
    Node<T>* handle_underflow(Node<T>* node, std::vector<Node<T>*>& siblings) {
        throw not_implemented;
    }
    Node<T>* adjust_tree(Node<T>* node, Node<T>* new_node, std::vector<Node<T>*>& siblings) {
        throw not_implemented;
    }
    void condense_tree(Node<T>* node, Node<T>* del_node, std::vector<Node<T>*>& siblings) {
        throw not_implemented;
    }
};

}  // namespace hilbert