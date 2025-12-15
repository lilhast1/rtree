#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "rtree/rtree.h"
#include "rtree_hilbert/hilbert_curve.h"

using ll = long long;
using Point = std::vector<ll>;
using namespace std::literals::string_literals;

namespace hilbert {

const auto not_implemented = std::logic_error("Not implemented"s);

struct Rectangle {
    Point lower;
    Point higher;

    Rectangle(Point lo, Point hi) : lower(std::move(lo)), higher(std::move(hi)) {
        if (lower.size() != hi.size()) {
            throw std::domain_error("Rectangle dimensions mismatch");
        }
    }

    Point get_center() const {
        auto center(lower);
        for (int i = 0; i < lower.size(); i++) {
            center[i] = (lower[i] + higher[i]) / 2;
        }
        return center;
    }
    Point get_lower() const { return lower; }
    Point get_upper() const { return higher; }
    bool intersects(const Rectangle& rect) const {
        bool intersect = true;

        if (this->lower.size() != rect.lower.size()) {
            throw std::runtime_error("The two rectangles do not have the same dimension.");
        }

        for (size_t i = 0; intersect && i < this->lower.size(); ++i) {
            intersect = (this->lower[i] <= rect.higher[i] && this->higher[i] >= rect.lower[i]);
        }

        return intersect;
    }
    bool contains(const Rectangle& rect) const {
        bool contains = true;

        if (this->lower.size() != rect.lower.size()) {
            throw std::runtime_error("The two rectangles do not have the same dimension.");
        }

        for (size_t i = 0; i < this->lower.size() && contains == true; ++i) {
            contains = (this->lower[i] <= rect.lower[i] && rect.lower[i] <= this->higher[i]
                        && this->lower[i] <= rect.higher[i] && rect.higher[i] <= this->higher[i]);
        }

        return contains;
    }
    bool operator==(const Rectangle& other) const {
        bool equal = true;

        if (this->lower.size() != other.lower.size()) {
            throw std::runtime_error("The two rectangles do not have the same dimension.");
        }

        for (size_t i = 0; equal && i < this->lower.size(); ++i) {
            equal = (this->lower[i] == other.lower[i] && this->higher[i] == other.higher[i]);
        }

        return equal;
    }
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
    bool leaf;
    Node* parent;
    Node* prev_sibling;
    Node* next_sibling;

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