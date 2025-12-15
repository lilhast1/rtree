#include <climits>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <set>
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
    virtual ll get_lhv() = 0;
    virtual Rectangle& get_mbr() = 0;
    virtual bool is_leaf() = 0;
    virtual ~NodeEntry() = default;
};

template <typename T>
struct LeafEntry : NodeEntry<T> {
    Rectangle mbr;
    T* elem;
    ll lhv;
    LeafEntry(Rectangle mbr, ll lhv) : lhv(lhv), mbr(std::move(mbr)) {}
    ll get_lhv() { return lhv; }
    bool is_leaf() { return true; }
    Rectangle& get_mbr() { return mbr; }
};

template <typename T>
struct InnerNode : NodeEntry<T> {
    Node<T>* node;
    InnerNode(Node<T>* node) : node(node) {}
    Node<T*> get_node() { return node; }
    bool is_leaf() { return false; }
    Rectangle& get_mbr() { return node->get_mbr(); }
    ll get_lhv() { return node->get_lhv(); }
};

template <typename T>
struct nodeEntryComparison {
    bool operator()(const NodeEntry<T>* first, const NodeEntry<T>* second) {
        auto l = first->get_lhv();
        auto r = second->get_lhv();
        return l < r;
    }
};

template <typename T>
using EntryMultiSet = std::multiset<NodeEntry<T>*, nodeEntryComparison<T>>;

template <typename T>
struct Node {
    bool leaf;
    Node* parent;
    Node* prev_sibling;
    Node* next_sibling;
    HilbertCurve curve;
    int min_entries, max_entries;
    EntryMultiSet<T> entries;
    Rectangle mbr;
    ll lhv;
    int dims;

    Node(int min_entries, int max_entries, int dims, int bits)
        : leaf(false),
          parent(nullptr),
          prev_sibling(nullptr),
          next_sibling(nullptr),
          curve(HilbertCurve(bits, dims)),
          min_entries(min_entries),
          max_entries(max_entries),
          dims(dims),
          mbr(Point(dims), Point(dims)),
          lhv(0) {}
    bool overflow() { return entries.size() >= max_entries; }
    bool underflow() { return entries.size() <= min_entries; }
    Node<T>* get_prev_siblings() { return prev_sibling; }
    Node<T>* get_next_siblings() { return next_sibling; }
    void set_next_siblings(Node<T>* next) { next_sibling = next; }
    void set_prev_siblings(Node<T>* prev) { prev_sibling = prev; }
    Node<T>* get_parent() { return parent; }
    void set_parent(Node<T>* new_par) { parent = new_par; }
    bool is_leaf() { return leaf; }
    void set_leaf(bool val) { leaf = val; }
    ll get_lhv() { return lhv; }
    Rectangle& get_mbr() { return mbr; }
    void insert_leaf_entry(LeafEntry<T>* entry) {
        if (!this->leaf) {
            throw std::runtime_error("The current node is not a leaf node.");
        }

        if (overflow()) {
            throw std::runtime_error("The node is overflowing.");
        }

        entries.insert(entry);
    }
    void insert_inner_entry(InnerNode<T>* entry) {
        if (this->leaf) {
            throw std::runtime_error("The current node is a leaf node.");
        }

        if (this->overflow()) {
            throw std::runtime_error("The node is overflowing.");
        }

        auto it = this->entries.insert(entry);

        // The parent of the node the entry is pointing to will be the current node.
        entry->get_node()->set_parent(this);

        assert(!this->next_sibling
               || (this->next_sibling && this->leaf == this->next_sibling->leaf));
        assert(!this->prev_sibling
               || (this->prev_sibling && this->leaf == this->prev_sibling->leaf));

        auto aux = it;
        Node<T>* nextSib = NULL;
        Node<T>* prevSib = NULL;

        if (it != this->entries.begin()) {
            auto prevIt = it;
            prevIt--;
            prevSib = dynamic_cast<InnerNode<T>*>(*prevIt)->getNode();
        }
        entry->get_node()->prev_sibling = prevSib;
        if (prevSib != NULL) {
            prevSib->next_sibling = entry->getNode();
            assert(entry->getNode()->leaf == prevSib->leaf);
        }

        aux = this->entries.end();
        aux--;
        if (it != aux) {
            auto nextIt = it;
            nextIt++;
            nextSib = dynamic_cast<InnerNode<T>*>(*nextIt)->getNode();
        }
        entry->getNode()->nextSibling = nextSib;
        if (nextSib != NULL) {
            nextSib->prev_sibling = entry->get_node();
            assert(entry->getNode()->leaf == nextSib->leaf);
        }
    }
    void remove_leaf_entry(const Rectangle& rect) {
        if (!leaf)
            throw std::runtime_error("Can't remove inner node as child node");
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            auto entry = *it;
            if (dynamic_cast<LeafEntry<T>*>(entry)->get_mbr() == rect) {
                entries.erase(it);
                break;
            }
        }
    }
    void remove_inner_entry(InnerNode<T>* child) {
        if (leaf)
            throw std::runtime_error("Can't remove child node as inner node");
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            auto entry = *it;
            if (dynamic_cast<InnerNode<T>*>(entry)->get_node() == child) {
                entries.erase(it);
                break;
            }
        }
    }
    void adjust_mbr() {
        Point lo(dims, INT64_MAX);
        Point hi(dims, INT64_MIN);

        for (auto& entry : entries) {
            auto rect = entry->get_mbr();
            for (int i = 0; i < dims; i++) {
                if (rect.lower[i] < lo[i])
                    lo[i] = rect.lower[i];
                if (rect.higher[i] > hi[i])
                    hi[i] = rect.higher[i];
            }
        }
        mbr = Rectangle(lo, hi);
    }
    void adjust_lhv() {
        lhv = INT64_MIN;
        for (auto& entry : entries) {
            if (entry->get_lhv() > lhv)
                lhv = entry->get_lhv();
        }
    }
    std::vector<Node<T>*> get_siblings(int num) {
        std::vector<Node<T>*> result;
        result.push_back(this);

        auto right = next_sibling;

        while (result.size() < num && right != NULL) {
            result.push_back(right);
            right = right->next_sibling;
        }

        return result;
    }
    EntryMultiSet<T>& get_entries() { return entries; }
    void reset_entries() {
        entries.clear();
        lhv = 0;
    }
};

template <typename T>
class RTree {
    Node<T>* root;

   public:
    RTree() { throw not_implemented; }
    ~RTree() { throw not_implemented; }

    std::vector<T*> search(const Rectangle& search_rect) {
        auto _rez = _search(root, search_rect);
        std::vector<T*> result;
        for (auto entry : _rez) {
            result.push_back(entry->elem);
        }
    }
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

    std::vector<LeafEntry<T>*> _search(Node<T>* subtree, const Rectangle& rect) {
        std::vector<LeafEntry<T>*> result;
        std::vector<LeafEntry<T>*> aux;
        auto entries = subtree->get_entries();
        auto it = entries.begin();

        if (subtree->is_leaf()) {
            for (it = entries.begin(); it != entries.end(); ++it) {
                if ((*it)->get_mbr()->intersects(rect)) {
                    result.push_back(dynamic_cast<LeafEntry<T>*>(*it));
                }
            }
        } else {
            for (it = entries.begin(); it != entries.end(); ++it) {
                if ((*it)->get_mbr()->intersects(rect)) {
                    aux = _search(dynamic_cast<InnerNode<T>*>(*it)->getNode(), rect);
                    result.insert(result.end(), aux.begin(), aux.end());
                }
            }
        }

        return result;
    }
};

}  // namespace hilbert