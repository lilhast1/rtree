#include <climits>
#include <cstddef>
#include <cstdint>
#include <deque>
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
        if (lo.size() != hi.size()) {
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
    virtual ll get_lhv() const = 0;
    virtual Rectangle& get_mbr() const = 0;
    virtual bool is_leaf() const = 0;
    virtual ~NodeEntry() = default;
};

template <typename T>
struct LeafEntry : NodeEntry<T> {
    mutable Rectangle mbr;
    T* elem;
    ll lhv;
    LeafEntry(Rectangle mbr, ll lhv, T* elem) : lhv(lhv), mbr(std::move(mbr)), elem(elem) {}
    ll get_lhv() const { return lhv; }
    bool is_leaf() const { return true; }
    Rectangle& get_mbr() const { return mbr; }
};

template <typename T>
struct InnerNode : NodeEntry<T> {
    Node<T>* node;
    InnerNode(Node<T>* node) : node(node) {}
    Node<T>* get_node() { return node; }
    bool is_leaf() const { return false; }
    Rectangle& get_mbr() const { return node->get_mbr(); }
    ll get_lhv() const { return node->get_lhv(); }
};

template <typename T>
struct nodeEntryComparison {
    bool operator()(NodeEntry<T>* first, NodeEntry<T>* second) const {
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
    HilbertCurve& curve;
    int min_entries, max_entries;
    EntryMultiSet<T> entries;
    Rectangle mbr;
    ll lhv;
    int dims;

    Node(int min_entries, int max_entries, HilbertCurve& curve)
        : leaf(false),
          parent(nullptr),
          prev_sibling(nullptr),
          next_sibling(nullptr),
          curve(curve),
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
            prevSib = dynamic_cast<InnerNode<T>*>(*prevIt)->get_node();
        }
        entry->get_node()->prev_sibling = prevSib;
        if (prevSib != NULL) {
            prevSib->next_sibling = entry->get_node();
            assert(entry->get_node()->leaf == prevSib->leaf);
        }

        aux = this->entries.end();
        aux--;
        if (it != aux) {
            auto nextIt = it;
            nextIt++;
            nextSib = dynamic_cast<InnerNode<T>*>(*nextIt)->get_node();
        }
        entry->get_node()->next_sibling = nextSib;
        if (nextSib != NULL) {
            nextSib->prev_sibling = entry->get_node();
            assert(entry->get_node()->leaf == nextSib->leaf);
        }
    }
    void remove_leaf_entry(const Rectangle& rect) {
        if (!leaf)
            throw std::runtime_error("Can't remove inner node as child node");
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            auto entry = *it;
            if (dynamic_cast<LeafEntry<T>*>(entry)->get_mbr() == rect) {
                entries.erase(it);
                // delete entry list ne mora
                break;
            }
        }
    }
    void remove_inner_entry(Node<T>* child) {
        if (leaf)
            throw std::runtime_error("Can't remove child node as inner node");
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            auto entry = *it;
            if (dynamic_cast<InnerNode<T>*>(entry)->get_node() == child) {
                entries.erase(it);
                // delete entry
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
    std::deque<Node<T>*> get_siblings(int num) {
        std::deque<Node<T>*> result;
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
    int min_entries;
    int max_entries;
    HilbertCurve curve;

   public:
    RTree(int min, int max, int dims, int bits)
        : min_entries(min), max_entries(max), curve(bits, dims), root(nullptr) {}
    ~RTree() { delete root; }

    std::deque<T*> search(const Rectangle& search_rect) {
        auto rez = _search(root, search_rect);
        std::deque<T*> result;
        for (auto entry : rez) {
            result.push_back(entry->elem);
        }
        return result;
    }
    void insert(const Rectangle& rect,
                T* elem) {  // Compute the hilbert value of the center of the rectangle
        ll h = curve.index(rect.get_center());

        // List which contains:
        // 1. The leaf into which the new entry was inserted
        // 2. The siblings of the leaf node (if an overflow happened)
        // 3. The newly created node(if one was created)
        std::deque<Node<T>*> out_siblings;

        // Create a new entry to insert
        auto* newEntry = new LeafEntry<T>(rect, h, elem);

        // Node that is created if an overflow occurs.
        Node<T>* NN = nullptr;

        // Find a leaf into which we can insert this value
        Node<T>* L = choose_leaf(this->root, h);

        // if the leaf is not full, we can simply insert the value in this node
        if (!L->overflow()) {
            L->insert_leaf_entry(newEntry);
            // After insertion, we must adjust the LHV and the MBR
            L->adjust_lhv();
            L->adjust_mbr();
            // The siblings list will only contain L.
            out_siblings.push_back(L);
        } else {
            // Handle the overflow of the node
            NN = handle_overflow(L, newEntry, out_siblings);
        }

        this->root = adjust_tree(this->root, L, NN, out_siblings);
    }
    void remove(const Rectangle& rect) {
        Node<T>* DL = nullptr;

        // List of siblings affected by the removal of an entry
        std::deque<Node<T>*> out_siblings;

        // Find the node containing the entry
        auto L = exactSearch(this->root, rect);

        // If there was a match
        if (L != nullptr) {
            // Remove the entry from the node
            L->remove_leaf_entry(rect);

            if (L->underflow()) {
                // DL contains a pointer to the node that was deleted or NULL if no node was deleted
                DL = handle_underflow(L, out_siblings);
            }

            condense_tree(L, DL, out_siblings);
        }
    }

   private:
    Node<T>* choose_leaf(Node<T>* node, ll hilbert_value) {
        if (node->is_leaf()) {
            return node;
        } else {
            for (auto it = node->get_entries().begin(); it != node->get_entries().end(); ++it) {
                // Choose the entry that has the LHV larger than the inserted value
                if ((*it)->get_lhv() >= hilbert_value) {
                    assert(!(*it)->is_leaf());
                    return choose_leaf(dynamic_cast<InnerNode<T>*>(*it)->get_node(), hilbert_value);
                }
            }
            auto it = node->get_entries().end();
            it--;

            return choose_leaf(dynamic_cast<InnerNode<T>*>(*it)->get_node(), hilbert_value);
        }
    }

    void redistribute_entries(EntryMultiSet<T>& entries, std::deque<Node<T>*>& siblings) {
        auto batch_size = (ll)std::ceil((double)entries.size() / (double)siblings.size());

        auto siblings_it = siblings.begin();

        ll current_batch = 0;

        for (auto entry_it = entries.begin(); entry_it != entries.end(); entry_it++) {
            if ((*entry_it)->is_leaf()) {
                (*siblings_it)->insert_leaf_entry(dynamic_cast<LeafEntry<T>*>(*entry_it));
            } else {
                (*siblings_it)->insert_inner_entry(dynamic_cast<InnerNode<T>*>(*entry_it));
            }

            current_batch++;

            // If the current sibling has received its share,
            // adjust the MBR and LHV of the current node and
            // move to the next sibling and reset that batch counter
            if (current_batch == batch_size) {
                current_batch = 0;
                (*siblings_it)->adjust_lhv();
                (*siblings_it)->adjust_mbr();
                siblings_it++;
            }
        }

        if (siblings_it != siblings.end()) {
            (*siblings_it)->adjust_lhv();
            (*siblings_it)->adjust_mbr();
        }
    }
    Node<T>* handle_overflow(Node<T>* target, NodeEntry<T>* entry,
                             std::deque<Node<T>*>& out_siblings) {
        EntryMultiSet<T> entries;

        // Node that will be created if there is no room for redistribution
        Node<T>* newNode = nullptr;

        // Position of the target node in the siblings list
        auto targetPos = out_siblings.begin();

        // The list will contain the siblings of the target
        out_siblings = target->get_siblings(2);

        // Insert the new entry in the set of entries that have to be redistributed
        entries.insert(entry);

        // Copy the entries of the siblings node into the set of entries that must be redistributed
        for (auto it = out_siblings.begin(); it != out_siblings.end(); ++it) {
            assert((*it)->is_leaf() == entry->is_leaf());
            entries.insert((*it)->get_entries().begin(), (*it)->get_entries().end());
            // Clear the entries in each node
            (*it)->reset_entries();

            if (*it == target) {
                targetPos = it;
            }
        }

        // If there is not enough room, create e new node
        if (entries.size() > out_siblings.size() * max_entries) {
            // The new node is a sibling of the target.
            newNode = new Node<T>(min_entries, max_entries, curve);
            // The new node will be a leaf only if its entries are leaf entries
            newNode->set_leaf(entry->is_leaf());

            // The previous sibling of the new node will be the
            // previous sibling of the first node in the set of siblings
            auto prevSib = target->get_prev_siblings();
            newNode->set_prev_siblings(prevSib);
            if (prevSib != nullptr) {
                // assert(prevSib->isLeaf() == newNode->isLeaf());
                prevSib->set_next_siblings(newNode);
            }

            // The next sibling of the new node will be the next sibling
            // of the first node
            newNode->set_next_siblings(target);
            target->set_prev_siblings(newNode);

            // Insert the new node in the list of cooperating siblings
            out_siblings.insert(targetPos, newNode);
        }

        // Redistribute the entries and adjust the nodes
        redistribute_entries(entries, out_siblings);

        for (auto it = out_siblings.begin(); it != out_siblings.end(); ++it) {
            assert((*it)->is_leaf() == entry->is_leaf());
        }

        return newNode;
    }
    Node<T>* handle_underflow(Node<T>* target, std::deque<Node<T>*>& out_siblings) {
        EntryMultiSet<T> entries;
        // Pointer to the deleted node if any
        Node<T>* removed = nullptr;

        // This will contain the target node and at most SIBLINGS_NO of its siblings
        out_siblings = target->get_siblings(2 + 1);

        for (auto it = out_siblings.begin(); it != out_siblings.end(); ++it) {
            entries.insert((*it)->get_entries().begin(), (*it)->get_entries().end());
            (*it)->reset_entries();
        }

        // The nodes are underflowing and our node is not the root
        if (entries.size() < out_siblings.size() * min_entries && target->get_parent() != nullptr) {
            removed = out_siblings.front();
            Node<T>* prevSib = removed->get_prev_siblings();
            Node<T>* nextSib = removed->get_next_siblings();

            if (prevSib != NULL) {
                prevSib->set_next_siblings(nextSib);
            }

            if (nextSib != NULL) {
                nextSib->set_prev_siblings(prevSib);
            }

            out_siblings.pop_front();
        }

        redistribute_entries(entries, out_siblings);

        return removed;
    }
    Node<T>* adjust_tree(Node<T>* root, Node<T>* N, Node<T>* NN, std::deque<Node<T>*>& siblings) {
        // Node that is created if the parent needs to be split
        Node<T>* PP = NULL;

        // The new root

        auto newRoot = root;

        std::deque<Node<T>*> newSiblings;

        // Set of parent nodes of the sibling nodes
        std::set<Node<T>*> P;

        // Flag determining if a new node was created
        std::set<Node<T>*> S;
        S.insert(siblings.begin(), siblings.end());

        bool ok = true;

        while (ok) {
            // The parent of the node being updated
            Node<T>* Np = N->get_parent();

            if (Np == nullptr) {
                // N is the root, we must exit the loop
                ok = false;

                // If the root has overflown, a new root will be created
                if (NN != nullptr) {
                    auto n = new InnerNode<T>(N);
                    auto nn = new InnerNode<T>(NN);

                    newRoot = new Node<T>(min_entries, max_entries, curve);
                    newRoot->insert_inner_entry(n);
                    newRoot->insert_inner_entry(nn);
                }

                // Adjust the LHV and MBR of the root
                newRoot->adjust_lhv();
                newRoot->adjust_mbr();
            } else {
                if (NN != nullptr) {
                    // Insert the entry containing the newly created node into the tree
                    auto nn = new InnerNode<T>(NN);
                    // try to insert the new node into the parent
                    if (!Np->overflow()) {
                        Np->insert_inner_entry(nn);

                        // After insertion, we must adjust the LHV and MBR
                        Np->adjust_lhv();
                        Np->adjust_mbr();
                        // The new siblings list will only contain the node into which
                        // the new entry was inserted
                        newSiblings.push_back(Np);
                    } else {
                        // All the nodes in newsiblings will have their LHV and MBR adjusted
                        PP = handle_overflow(Np, nn, newSiblings);
                    }
                } else {
                    // the new entry was inserted
                    newSiblings.push_back(Np);
                }

                for (auto node = S.begin(); node != S.end(); ++node) {
                    P.insert((*node)->get_parent());
                }

                for (auto node = P.begin(); node != P.end(); ++node) {
                    (*node)->adjust_lhv();
                    (*node)->adjust_mbr();
                }

                N = Np;
                NN = PP;

                S.clear();
                P.clear();

                S.insert(newSiblings.begin(), newSiblings.end());
            }
        }

        return newRoot;
    }
    void condense_tree(Node<T>* node, Node<T>* del_node, std::deque<Node<T>*>& siblings) {
        std::deque<Node<T>*> new_siblings;

        std::set<Node<T>*> s, p;
        s.insert(siblings.begin(), siblings.end());

        bool stop_flag = false;

        while (!stop_flag) {
            auto np = node->get_parent();
            Node<T>* dpparent = nullptr;
            if (np == nullptr) {
                stop_flag = true;
                auto main_entry =
                    dynamic_cast<InnerNode<T>*>(*node->get_entries().begin())->get_node();
                auto data = main_entry->get_entries();
                node->reset_entries();
                if (main_entry->is_leaf()) {
                    node->set_leaf(true);
                    for (auto it = data.begin(); it != data.end(); it++) {
                        node->insert_leaf_entry(dynamic_cast<LeafEntry<T>*>(*it));
                    }
                } else {
                    for (auto it = data.begin(); it != data.end(); it++) {
                        node->insert_inner_entry(dynamic_cast<InnerNode<T>*>(*it));
                    }
                }
                node->adjust_lhv();
                node->adjust_mbr();
            } else {
                if (del_node != nullptr) {
                    auto dnparent = del_node->get_parent();
                    dnparent->remove_inner_entry(del_node);
                    if (dnparent->underflow()) {
                        dpparent = handle_underflow(dnparent, new_siblings);
                    } else {
                        new_siblings.push_back(dnparent);
                    }
                }
                new_siblings.push_back(np);
                for (auto it = s.begin(); it != s.end(); ++it) {
                    p.insert((*it)->get_parent());
                }

                for (auto it = p.begin(); it != p.end(); ++it) {
                    (*it)->adjust_lhv();
                    (*it)->adjust_mbr();
                }
                node = np;
                del_node = dpparent;
                s.clear();
                p.clear();
                s.insert(new_siblings.begin(), new_siblings.end());
            }
        }
    }

    Node<T>* exactSearch(Node<T>* subtree, const Rectangle& rect) {
        auto entries = subtree->get_entries();
        auto it = entries.begin();

        // There are no duplicates in the tree
        if (subtree->is_leaf()) {
            for (it = entries.begin(); it != entries.end(); ++it) {
                if (rect == (*it)->get_mbr()) {
                    return subtree;
                }
            }
        } else {
            for (it = entries.begin(); it != entries.end(); ++it) {
                if ((*it)->get_mbr().contains(rect)) {
                    auto res = exactSearch(dynamic_cast<InnerNode<T>*>(*it)->get_node(), rect);
                    if (res != nullptr) {
                        return res;
                    }
                }
            }
        }

        return nullptr;
    }

    std::deque<LeafEntry<T>*> _search(Node<T>* subtree, const Rectangle& rect) {
        std::deque<LeafEntry<T>*> result;
        std::deque<LeafEntry<T>*> aux;
        auto entries = subtree->get_entries();
        auto it = entries.begin();

        if (subtree->is_leaf()) {
            for (it = entries.begin(); it != entries.end(); ++it) {
                if ((*it)->get_mbr().intersects(rect)) {
                    result.push_back(dynamic_cast<LeafEntry<T>*>(*it));
                }
            }
        } else {
            for (it = entries.begin(); it != entries.end(); ++it) {
                if ((*it)->get_mbr().intersects(rect)) {
                    // TODO: moze se optimizirati da se izbjegne kopiranja i to!
                    aux = _search(dynamic_cast<InnerNode<T>*>(*it)->get_node(), rect);
                    result.insert(result.end(), aux.begin(), aux.end());
                }
            }
        }

        return result;
    }
};

}  // namespace hilbert