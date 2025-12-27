/**
 * @file hilbert_rtree.h
 * @brief Hilbert R-Tree implementation for multidimensional rectangles.
 *
 * This file implements a Hilbert-curve-ordered R-Tree supporting
 * insertion, deletion, search, overflow/underflow handling, and
 * sibling chains.
 */

#include <climits>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <iostream>
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

/**
 * @brief Represents an axis-aligned rectangle in d-dimensional space.
 */
struct Rectangle {
    Point lower;   ///< lower-left corner
    Point higher;  ///< upper-right corner
                   /**
                    * @brief Construct a rectangle from lower and upper corners.
                    * @param lo lower point
                    * @param hi upper point
                    * @throws std::domain_error if dimensions mismatch
                    */
    Rectangle(Point lo, Point hi) : lower(std::move(lo)), higher(std::move(hi)) {
        if (lo.size() != hi.size()) {
            throw std::domain_error("Rectangle dimensions mismatch");
        }
    }
    /** @brief Get the center of the rectangle */
    Point get_center() const {
        auto center(lower);
        for (size_t i = 0; i < lower.size(); i++) {
            center[i] = (lower[i] + higher[i]) / 2;
        }
        return center;
    }
    /** @brief Get the lower corner */
    Point get_lower() const { return lower; }
    /** @brief Get the upper corner */
    Point get_upper() const { return higher; }
    /**
     * @brief Check if this rectangle intersects another
     * @param rect other rectangle
     * @return true if intersects
     * @throws std::runtime_error if dimensions mismatch
     */
    bool intersects(const Rectangle& rect) const {
        if (this->lower.size() != rect.lower.size()) {
            throw std::runtime_error("The two rectangles do not have the same dimension.");
        }

        for (size_t i = 0; i < this->lower.size(); ++i) {
            if (this->lower[i] > rect.higher[i] || this->higher[i] < rect.lower[i]) {
                return false;
            }
        }
        return true;
    }
    /**
     * @brief Check if this rectangle contains another
     * @param rect other rectangle
     * @return true if fully contains
     * @throws std::runtime_error if dimensions mismatch
     */
    bool contains(const Rectangle& rect) const {
        if (this->lower.size() != rect.lower.size()) {
            throw std::runtime_error("The two rectangles do not have the same dimension.");
        }

        for (size_t i = 0; i < this->lower.size(); ++i) {
            if (this->lower[i] > rect.lower[i] || rect.higher[i] > this->higher[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator==(const Rectangle& other) const {
        if (this->lower.size() != other.lower.size()) {
            throw std::runtime_error("The two rectangles do not have the same dimension.");
        }

        for (size_t i = 0; i < this->lower.size(); ++i) {
            if (this->lower[i] != other.lower[i] || this->higher[i] != other.higher[i]) {
                return false;
            }
        }
        return true;
    }
};

template <typename T>
struct Node;

/**
 * @brief Base interface for node entries (leaf or inner node).
 */
template <typename T>
struct NodeEntry {
    /** @brief Get Largest Hilbert Value (LHV) */
    virtual ll get_lhv() const = 0;
    /** @brief Get Minimum Bounding Rectangle (MBR) */
    virtual Rectangle& get_mbr() const = 0;
    /** @brief Returns true if this is a leaf entry */
    virtual bool is_leaf() const = 0;
    virtual ~NodeEntry() = default;
};

/**
 * @brief Leaf entry containing a single element.
 */
template <typename T>
struct LeafEntry : NodeEntry<T> {
    mutable Rectangle mbr;  ///< bounding rectangle
    T* elem;                ///< element pointer
    ll lhv;                 ///< Hilbert value
    LeafEntry(Rectangle mbr, ll lhv, T* elem) : lhv(lhv), mbr(std::move(mbr)), elem(elem) {}
    ll get_lhv() const { return lhv; }
    bool is_leaf() const { return true; }
    Rectangle& get_mbr() const { return mbr; }
};

/**
 * @brief Inner node entry pointing to a child node.
 */
template <typename T>
struct InnerNode : NodeEntry<T> {
    Node<T>* node;  ///< child node
    InnerNode(Node<T>* node) : node(node) {}
    Node<T>* get_node() { return node; }
    bool is_leaf() const { return false; }
    Rectangle& get_mbr() const { return node->get_mbr(); }
    ll get_lhv() const { return node->get_lhv(); }
};

/**
 * @brief Comparison for node entries by LHV.
 * Ensures uniqueness by comparing pointer if LHV is equal.
 */
template <typename T>
struct nodeEntryComparison {
    bool operator()(NodeEntry<T>* first, NodeEntry<T>* second) const {
        auto l = first->get_lhv();
        auto r = second->get_lhv();
        if (l != r)
            return l < r;
        // When LHV is equal, compare pointers to ensure uniqueness
        return first < second;
    }
};

/** @brief Set of node entries with custom comparator */
template <typename T>
using EntrySet = std::set<NodeEntry<T>*, nodeEntryComparison<T>>;

/**
 * @brief Node in the R-Tree (leaf or inner)
 */
template <typename T>
struct Node {
    bool leaf;  ///< is leaf node
    Node* parent;
    Node* prev_sibling;
    Node* next_sibling;
    HilbertCurve& curve;
    int min_entries, max_entries;
    EntrySet<T> entries;  ///< set of node entries
    Rectangle mbr;        ///< bounding rectangle of node
    ll lhv;               ///< largest Hilbert value
    int dims;             ///< number of dimensions

    Node(int min_entries, int max_entries, HilbertCurve& curve)
        : leaf(false),
          parent(nullptr),
          prev_sibling(nullptr),
          next_sibling(nullptr),
          curve(curve),
          min_entries(min_entries),
          max_entries(max_entries),
          dims(curve.get_dim()),
          mbr(Point(curve.get_dim()), Point(curve.get_dim())),
          lhv(0) {}

    ~Node() {
        for (auto entry : entries) {
            if (entry->is_leaf()) {
                delete static_cast<LeafEntry<T>*>(entry);
            } else {
                auto inner = static_cast<InnerNode<T>*>(entry);
                // FIX: Don't recursively delete child nodes here - they're managed separately
                // Only delete the InnerNode wrapper
                delete inner;
            }
        }
    }

    bool overflow() const { return entries.size() > static_cast<size_t>(max_entries); }
    bool underflow() const { return entries.size() < static_cast<size_t>(min_entries); }
    Node<T>* get_prev_siblings() { return prev_sibling; }
    Node<T>* get_next_siblings() { return next_sibling; }
    void set_next_siblings(Node<T>* next) { next_sibling = next; }
    void set_prev_siblings(Node<T>* prev) { prev_sibling = prev; }
    Node<T>* get_parent() { return parent; }

    void set_parent(Node<T>* new_par) {
        if (new_par == this) {
            std::cerr << "ERROR: Attempting to set node as its own parent!\n";
            throw std::runtime_error("Self-parent detected");
        }
        parent = new_par;
    }

    bool is_leaf() const { return leaf; }
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
        entries.insert(entry);  // set::insert will not add duplicates
    }

    void insert_inner_entry(InnerNode<T>* entry) {
        if (this->leaf) {
            throw std::runtime_error("The current node is a leaf node.");
        }
        if (this->overflow()) {
            throw std::runtime_error("The node is overflowing.");
        }

        auto result = this->entries.insert(entry);
        if (!result.second) {
            // Entry was already present
            return;
        }

        auto it = result.first;
        entry->get_node()->set_parent(this);

        Node<T>* nextSib = nullptr;
        Node<T>* prevSib = nullptr;

        if (it != this->entries.begin()) {
            auto prevIt = it;
            --prevIt;
            prevSib = dynamic_cast<InnerNode<T>*>(*prevIt)->get_node();
        }

        entry->get_node()->set_prev_siblings(prevSib);
        if (prevSib != nullptr) {
            prevSib->set_next_siblings(entry->get_node());
        }

        auto lastIt = this->entries.end();
        --lastIt;
        if (it != lastIt) {
            auto nextIt = it;
            ++nextIt;
            nextSib = dynamic_cast<InnerNode<T>*>(*nextIt)->get_node();
        }

        entry->get_node()->set_next_siblings(nextSib);
        if (nextSib != nullptr) {
            nextSib->set_prev_siblings(entry->get_node());
        }
    }

    void remove_leaf_entry(const Rectangle& rect) {
        if (!leaf)
            throw std::runtime_error("Can't remove inner node as child node");
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            auto entry = *it;
            if (dynamic_cast<LeafEntry<T>*>(entry)->get_mbr() == rect) {
                entries.erase(it);
                delete entry;
                return;
            }
        }
    }

    void remove_inner_entry(Node<T>* child) {
        if (leaf)
            throw std::runtime_error("Can't remove child node as inner node");
        for (auto it = entries.begin(); it != entries.end(); ++it) {
            auto entry = *it;
            auto inner = dynamic_cast<InnerNode<T>*>(entry);
            if (inner && inner->get_node() == child) {
                entries.erase(it);
                delete entry;  // Only delete the wrapper
                return;
            }
        }
    }

    void adjust_mbr() {
        if (entries.empty()) {
            mbr = Rectangle(Point(dims, 0), Point(dims, 0));
            return;
        }

        Point lo(dims, INT64_MAX);
        Point hi(dims, INT64_MIN);

        for (auto& entry : entries) {
            auto& rect = entry->get_mbr();
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

    std::deque<Node<T>*> get_siblings(size_t num) {
        std::deque<Node<T>*> result;
        std::set<Node<T>*> visited;

        result.push_back(this);
        visited.insert(this);

        auto right = next_sibling;
        size_t iterations = 0;
        const size_t MAX_ITERATIONS = 10000;

        while (result.size() < num && right != nullptr && iterations < MAX_ITERATIONS) {
            iterations++;

            // Check for obvious corruption
            if (right == this) {
                break;  // Self-loop
            }

            if (visited.find(right) != visited.end()) {
                break;  // Cycle detected
            }

            // Verify parent relationship - siblings should have same parent
            if (right->get_parent() != this->get_parent()) {
                break;  // Different parent - stop here
            }

            result.push_back(right);
            visited.insert(right);
            right = right->next_sibling;
        }

        return result;
    }

    EntrySet<T>& get_entries() { return entries; }

    void reset_entries() {
        if (!leaf) {
            for (auto entry : entries) {
                auto inner = dynamic_cast<InnerNode<T>*>(entry);
                if (inner && inner->get_node()) {
                    inner->get_node()->set_prev_siblings(nullptr);
                    inner->get_node()->set_next_siblings(nullptr);
                }
            }
        }
        entries.clear();
        lhv = 0;
    }
};

/**
 * @brief R-Tree class with Hilbert curve ordering.
 */
template <typename T>
class RTree {
    Node<T>* root;
    int min_entries;
    int max_entries;
    HilbertCurve curve;
    std::set<Node<T>*> all_nodes;  // Track all nodes for proper cleanup

   public:
    RTree(int min, int max, int dims, int bits)
        : min_entries(min), max_entries(max), curve(bits, dims), root(nullptr) {}

    ~RTree() {
        // Delete all nodes
        for (auto node : all_nodes) {
            delete node;
        }
    }
    /**
     * @brief Search all elements intersecting a rectangle.
     * @param search_rect search rectangle
     * @return deque of element pointers
     */
    std::deque<T*> search(const Rectangle& search_rect) {
        if (!root)
            return std::deque<T*>();

        auto rez = _search(root, search_rect);
        std::deque<T*> result;
        for (auto entry : rez) {
            result.push_back(entry->elem);
        }
        return result;
    }
    /**
     * @brief Insert an element with rectangle.
     * @param rect rectangle of element
     * @param elem pointer to element
     */
    void insert(const Rectangle& rect, T* elem) {
        ll h = curve.index(rect.get_center());
        if (this->root == nullptr) {
            this->root = new Node<T>(min_entries, max_entries, curve);
            all_nodes.insert(root);
            this->root->set_leaf(true);
        }

        std::deque<Node<T>*> out_siblings;
        auto* newEntry = new LeafEntry<T>(rect, h, elem);
        Node<T>* NN = nullptr;
        Node<T>* L = choose_leaf(this->root, h);

        if (L->entries.size() < static_cast<size_t>(max_entries)) {
            L->insert_leaf_entry(newEntry);
            L->adjust_lhv();
            L->adjust_mbr();
            out_siblings.push_back(L);
        } else {
            NN = handle_overflow(L, newEntry, out_siblings);
        }

        this->root = adjust_tree(this->root, L, NN, out_siblings);
    }
    /**
     * @brief Remove element by exact rectangle.
     * @param rect rectangle of element to remove
     */
    void remove(const Rectangle& rect) {
        if (!root)
            return;

        Node<T>* DL = nullptr;
        std::deque<Node<T>*> out_siblings;
        auto L = exactSearch(this->root, rect);

        if (L != nullptr) {
            L->remove_leaf_entry(rect);

            if (L->underflow() && L->get_parent() != nullptr) {
                DL = handle_underflow(L, out_siblings);
            }

            condense_tree(L, DL, out_siblings);
        }
    }

   private:
    Node<T>* choose_leaf(Node<T>* node, ll hilbert_value) {
        if (node->is_leaf()) {
            return node;
        }

        for (auto it = node->get_entries().begin(); it != node->get_entries().end(); ++it) {
            if ((*it)->get_lhv() >= hilbert_value) {
                return choose_leaf(dynamic_cast<InnerNode<T>*>(*it)->get_node(), hilbert_value);
            }
        }

        auto it = node->get_entries().end();
        --it;
        return choose_leaf(dynamic_cast<InnerNode<T>*>(*it)->get_node(), hilbert_value);
    }

    void fix_cross_parent_sibling_links(std::deque<Node<T>*>& siblings) {
        if (siblings.empty() || siblings[0]->is_leaf())
            return;

        for (size_t i = 0; i < siblings.size() - 1; i++) {
            Node<T>* current_parent = siblings[i];
            Node<T>* next_parent = siblings[i + 1];

            if (current_parent->get_entries().empty() || next_parent->get_entries().empty())
                continue;

            auto last_entry_it = current_parent->get_entries().end();
            --last_entry_it;
            auto last_inner = dynamic_cast<InnerNode<T>*>(*last_entry_it);
            if (!last_inner)
                continue;
            Node<T>* last_child = last_inner->get_node();

            auto first_entry_it = next_parent->get_entries().begin();
            auto first_inner = dynamic_cast<InnerNode<T>*>(*first_entry_it);
            if (!first_inner)
                continue;
            Node<T>* first_child = first_inner->get_node();

            // CRITICAL: Prevent cycles
            if (last_child == first_child || last_child == current_parent
                || first_child == next_parent || last_child->get_parent() == first_child
                || first_child->get_parent() == last_child) {
                continue;
            }

            last_child->set_next_siblings(first_child);
            first_child->set_prev_siblings(last_child);
        }
    }

    void redistribute_entries(EntrySet<T>& entries, std::deque<Node<T>*>& siblings) {
        if (siblings.empty() || entries.empty()) {
            return;
        }

        size_t batch_size = (entries.size() + siblings.size() - 1) / siblings.size();
        auto siblings_it = siblings.begin();
        size_t current_batch = 0;

        // Create vector for iteration, clear set to prevent reuse
        std::vector<NodeEntry<T>*> entries_vec(entries.begin(), entries.end());
        entries.clear();

        for (auto entry : entries_vec) {
            if (entry->is_leaf()) {
                (*siblings_it)->insert_leaf_entry(dynamic_cast<LeafEntry<T>*>(entry));
            } else {
                auto inner = dynamic_cast<InnerNode<T>*>(entry);
                // Direct insert without going through insert_inner_entry to avoid sibling link
                // updates
                (*siblings_it)->entries.insert(inner);
                inner->get_node()->set_parent(*siblings_it);
            }

            current_batch++;

            if (current_batch >= batch_size && siblings_it != siblings.end()) {
                (*siblings_it)->adjust_lhv();
                (*siblings_it)->adjust_mbr();
                ++siblings_it;
                current_batch = 0;

                if (siblings_it == siblings.end()) {
                    --siblings_it;
                }
            }
        }

        // Adjust last batch
        if (siblings_it != siblings.end()) {
            (*siblings_it)->adjust_lhv();
            (*siblings_it)->adjust_mbr();
        }

        // Fix child sibling links within each parent
        for (auto node : siblings) {
            if (!node->is_leaf() && !node->get_entries().empty()) {
                fix_child_sibling_links(node);
            }
        }

        // Fix cross-parent links
        fix_cross_parent_sibling_links(siblings);
    }

    void validate_and_fix_child_chain(Node<T>* parent) {
        if (parent == nullptr || parent->is_leaf() || parent->get_entries().empty()) {
            return;
        }

        // Collect all child nodes in order
        std::vector<Node<T>*> children;
        for (auto entry : parent->get_entries()) {
            auto inner = dynamic_cast<InnerNode<T>*>(entry);
            if (inner) {
                children.push_back(inner->get_node());
            }
        }

        if (children.empty()) {
            return;
        }

        // Clear all child sibling pointers
        for (auto child : children) {
            child->set_prev_siblings(nullptr);
            child->set_next_siblings(nullptr);
        }

        // Rebuild chain in order
        for (size_t i = 0; i < children.size(); ++i) {
            if (i > 0) {
                children[i]->set_prev_siblings(children[i - 1]);
                children[i - 1]->set_next_siblings(children[i]);
            }
        }
    }

    void fix_child_sibling_links(Node<T>* parent) { validate_and_fix_child_chain(parent); }

    Node<T>* handle_overflow(Node<T>* target, NodeEntry<T>* entry,
                             std::deque<Node<T>*>& out_siblings) {
        EntrySet<T> entries;
        Node<T>* newNode = nullptr;

        out_siblings = target->get_siblings(2);

        Node<T>* leftmost_external_prev = out_siblings.front()->get_prev_siblings();
        Node<T>* rightmost_external_next = out_siblings.back()->get_next_siblings();
        Node<T>* original_parent = target->get_parent();

        entries.insert(entry);

        for (auto node : out_siblings) {
            entries.insert(node->get_entries().begin(), node->get_entries().end());
            node->reset_entries();
            node->set_prev_siblings(nullptr);
            node->set_next_siblings(nullptr);
        }

        if (entries.size() > out_siblings.size() * static_cast<size_t>(max_entries)) {
            newNode = new Node<T>(min_entries, max_entries, curve);
            all_nodes.insert(newNode);
            newNode->set_leaf(entry->is_leaf());
            newNode->set_parent(original_parent);
            out_siblings.push_back(newNode);
        }

        redistribute_entries(entries, out_siblings);

        for (auto node : out_siblings) {
            node->set_parent(original_parent);
        }

        // Set up sibling chain for redistributed nodes
        for (size_t i = 0; i < out_siblings.size(); ++i) {
            if (i > 0) {
                out_siblings[i]->set_prev_siblings(out_siblings[i - 1]);
            } else {
                out_siblings[i]->set_prev_siblings(leftmost_external_prev);
            }

            if (i < out_siblings.size() - 1) {
                out_siblings[i]->set_next_siblings(out_siblings[i + 1]);
            } else {
                out_siblings[i]->set_next_siblings(rightmost_external_next);
            }
        }

        // Update external neighbors to point to the new chain
        if (leftmost_external_prev != nullptr) {
            // Make sure we're not creating a cycle
            if (leftmost_external_prev != out_siblings.front()) {
                leftmost_external_prev->set_next_siblings(out_siblings.front());
            }
        }

        if (rightmost_external_next != nullptr) {
            // Make sure we're not creating a cycle
            if (rightmost_external_next != out_siblings.back()) {
                rightmost_external_next->set_prev_siblings(out_siblings.back());
            }
        }

        return newNode;
    }

    Node<T>* handle_underflow(Node<T>* target, std::deque<Node<T>*>& out_siblings) {
        if (target == nullptr) {
            return nullptr;
        }

        out_siblings = target->get_siblings(3);

        if (out_siblings.empty()) {
            out_siblings.push_back(target);
            return nullptr;
        }

        // If we got fewer siblings than requested due to a cycle,
        // we can't safely redistribute - just return target as-is
        if (out_siblings.size() < 2) {
            return nullptr;
        }

        // STEP 1: Save external neighbors BEFORE touching anything
        Node<T>* leftmost_external_prev = out_siblings.front()->get_prev_siblings();
        Node<T>* rightmost_external_next = out_siblings.back()->get_next_siblings();

        // Verify these are actually external (not in our sibling group)
        std::set<Node<T>*> sibling_set(out_siblings.begin(), out_siblings.end());
        if (leftmost_external_prev != nullptr
            && sibling_set.find(leftmost_external_prev) != sibling_set.end()) {
            leftmost_external_prev = nullptr;  // Not external, it's in our group
        }
        if (rightmost_external_next != nullptr
            && sibling_set.find(rightmost_external_next) != sibling_set.end()) {
            rightmost_external_next = nullptr;  // Not external, it's in our group
        }

        Node<T>* original_parent = target->get_parent();

        // STEP 2: Collect ALL entries before making any changes
        EntrySet<T> entries;
        for (auto node : out_siblings) {
            entries.insert(node->get_entries().begin(), node->get_entries().end());
        }

        // CRITICAL: If no entries collected, something is wrong - abort
        if (entries.empty()) {
            std::cerr << "ERROR: No entries collected from siblings in underflow\n";
            return nullptr;
        }

        // STEP 3: Decide if we should remove a node
        Node<T>* removed = nullptr;
        bool should_remove =
            (out_siblings.size() > 1) && original_parent != nullptr
            && entries.size() < out_siblings.size() * static_cast<size_t>(min_entries);

        if (should_remove) {
            removed = out_siblings.front();
            out_siblings.pop_front();

            // Update left external if the removed node was connected to it
            if (removed->get_prev_siblings() == leftmost_external_prev) {
                leftmost_external_prev = removed->get_prev_siblings();
            }
        }

        // STEP 3b: After removal, check if we still have enough nodes
        if (out_siblings.empty()) {
            std::cerr << "ERROR: All siblings removed in underflow\n";
            if (removed != nullptr) {
                out_siblings.push_back(removed);
                removed = nullptr;
            }
            return nullptr;
        }

        // STEP 4: Completely disconnect all nodes in the group
        for (auto node : out_siblings) {
            node->reset_entries();
            node->set_prev_siblings(nullptr);
            node->set_next_siblings(nullptr);
        }

        if (removed != nullptr) {
            removed->reset_entries();
            removed->set_prev_siblings(nullptr);
            removed->set_next_siblings(nullptr);
        }

        // Also disconnect external neighbors from us
        if (leftmost_external_prev != nullptr) {
            leftmost_external_prev->set_next_siblings(nullptr);
        }
        if (rightmost_external_next != nullptr) {
            rightmost_external_next->set_prev_siblings(nullptr);
        }

        // STEP 5: Redistribute entries
        redistribute_entries(entries, out_siblings);

        // STEP 6: Set parent
        for (auto node : out_siblings) {
            if (node != nullptr) {
                node->set_parent(original_parent);
            }
        }

        // STEP 7: Build clean internal chain
        for (size_t i = 0; i < out_siblings.size(); ++i) {
            if (i > 0) {
                out_siblings[i]->set_prev_siblings(out_siblings[i - 1]);
            } else {
                out_siblings[i]->set_prev_siblings(nullptr);
            }

            if (i < out_siblings.size() - 1) {
                out_siblings[i]->set_next_siblings(out_siblings[i + 1]);
            } else {
                out_siblings[i]->set_next_siblings(nullptr);
            }
        }

        // STEP 8: Reconnect to external neighbors
        if (!out_siblings.empty()) {
            if (leftmost_external_prev != nullptr) {
                out_siblings.front()->set_prev_siblings(leftmost_external_prev);
                leftmost_external_prev->set_next_siblings(out_siblings.front());
            }

            if (rightmost_external_next != nullptr) {
                out_siblings.back()->set_next_siblings(rightmost_external_next);
                rightmost_external_next->set_prev_siblings(out_siblings.back());
            }
        }

        return removed;
    }

    Node<T>* adjust_tree(Node<T>* root, Node<T>* N, Node<T>* NN, std::deque<Node<T>*>& siblings) {
        Node<T>* PP = nullptr;
        auto newRoot = root;
        std::deque<Node<T>*> newSiblings;
        std::set<Node<T>*> P;
        std::set<Node<T>*> S;
        S.insert(siblings.begin(), siblings.end());

        int iteration = 0;

        while (true) {
            iteration++;
            if (iteration > 1000) {
                std::cerr << "ERROR: adjust_tree infinite loop detected!\n";
                throw std::runtime_error("Infinite loop in adjust_tree");
            }

            Node<T>* Np = N->get_parent();

            if (Np == nullptr) {
                if (NN != nullptr) {
                    auto n = new InnerNode<T>(N);
                    auto nn = new InnerNode<T>(NN);

                    newRoot = new Node<T>(min_entries, max_entries, curve);
                    all_nodes.insert(newRoot);
                    newRoot->insert_inner_entry(n);
                    newRoot->insert_inner_entry(nn);

                    // CRITICAL: Fix the new root's child chain
                    validate_and_fix_child_chain(newRoot);
                }

                newRoot->adjust_lhv();
                newRoot->adjust_mbr();
                break;
            } else {
                if (NN != nullptr) {
                    auto nn = new InnerNode<T>(NN);

                    if (Np->entries.size() < static_cast<size_t>(max_entries)) {
                        Np->insert_inner_entry(nn);
                        // CRITICAL: Fix parent's child chain after insertion
                        validate_and_fix_child_chain(Np);
                        Np->adjust_lhv();
                        Np->adjust_mbr();
                        newSiblings.push_back(Np);
                    } else {
                        PP = handle_overflow(Np, nn, newSiblings);
                        // After overflow, validate all affected parents
                        for (auto sibling : newSiblings) {
                            if (sibling && !sibling->is_leaf()) {
                                validate_and_fix_child_chain(sibling);
                            }
                        }
                    }
                } else {
                    newSiblings.push_back(Np);
                }

                for (auto node : S) {
                    if (node != nullptr && node->get_parent() != nullptr) {
                        P.insert(node->get_parent());
                    }
                }

                for (auto node : P) {
                    if (node != nullptr) {
                        node->adjust_lhv();
                        node->adjust_mbr();
                        // CRITICAL: Validate child chains after adjustments
                        if (!node->is_leaf()) {
                            validate_and_fix_child_chain(node);
                        }
                    }
                }

                N = Np;
                NN = PP;
                PP = nullptr;

                S.clear();
                P.clear();

                S.insert(newSiblings.begin(), newSiblings.end());
                newSiblings.clear();
            }
        }

        return newRoot;
    }

    void condense_tree(Node<T>* node, Node<T>* del_node, std::deque<Node<T>*>& siblings) {
        std::deque<Node<T>*> new_siblings;
        std::set<Node<T>*> s, p;
        s.insert(siblings.begin(), siblings.end());

        int safety_counter = 0;
        const int MAX_ITERATIONS = INT_MAX;

        while (true) {
            safety_counter++;
            if (safety_counter > MAX_ITERATIONS) {
                std::cerr << "ERROR: Too many iterations in condense_tree\n";
                break;
            }

            auto np = node->get_parent();
            Node<T>* dpparent = nullptr;

            if (np == nullptr) {
                // Root condensation
                if (!node->is_leaf() && node->get_entries().size() == 1) {
                    auto main_entry = dynamic_cast<InnerNode<T>*>(*node->get_entries().begin());
                    if (main_entry) {
                        auto main_node = main_entry->get_node();

                        // Verify main_node is still valid
                        if (main_node != nullptr && all_nodes.find(main_node) != all_nodes.end()) {
                            EntrySet<T> data;
                            data.insert(main_node->get_entries().begin(),
                                        main_node->get_entries().end());

                            main_node->reset_entries();
                            node->reset_entries();

                            if (main_node->is_leaf()) {
                                node->set_leaf(true);
                                for (auto entry : data) {
                                    node->insert_leaf_entry(dynamic_cast<LeafEntry<T>*>(entry));
                                }
                            } else {
                                for (auto entry : data) {
                                    node->insert_inner_entry(dynamic_cast<InnerNode<T>*>(entry));
                                }
                                // Fix child sibling chain after moving entries to root
                                validate_and_fix_child_chain(node);
                            }

                            all_nodes.erase(main_node);
                            delete main_entry;
                            delete main_node;
                        }
                    }
                }

                if (!node->get_entries().empty()) {
                    node->adjust_lhv();
                    node->adjust_mbr();
                }
                break;
            } else {
                // Handle deleted node
                if (del_node != nullptr) {
                    auto dnparent = del_node->get_parent();

                    // CRITICAL: Check if node still exists before accessing
                    bool node_exists = (all_nodes.find(del_node) != all_nodes.end());

                    if (node_exists && dnparent != nullptr
                        && all_nodes.find(dnparent) != all_nodes.end()) {
                        // Remove from parent
                        dnparent->remove_inner_entry(del_node);

                        // CRITICAL: Fix parent's child sibling chain after removal
                        validate_and_fix_child_chain(dnparent);

                        if (dnparent->underflow() && dnparent->get_parent() != nullptr) {
                            dpparent = handle_underflow(dnparent, new_siblings);
                        } else {
                            new_siblings.push_back(dnparent);
                        }
                    }

                    // CRITICAL: Clear all pointers TO the deleted node before freeing it
                    auto del_prev = del_node->get_prev_siblings();
                    auto del_next = del_node->get_next_siblings();

                    if (del_prev != nullptr && all_nodes.find(del_prev) != all_nodes.end()) {
                        del_prev->set_next_siblings(del_next);
                    }
                    if (del_next != nullptr && all_nodes.find(del_next) != all_nodes.end()) {
                        del_next->set_prev_siblings(del_prev);
                    }

                    del_node->set_prev_siblings(nullptr);
                    del_node->set_next_siblings(nullptr);

                    // Delete the node if it still exists
                    // (handle_underflow may have already erased it from all_nodes)
                    if (node_exists && all_nodes.find(del_node) != all_nodes.end()) {
                        all_nodes.erase(del_node);
                    }

                    // Always safe to delete - either it's still valid or already erased
                    delete del_node;
                    del_node = nullptr;
                }

                if (np != nullptr && all_nodes.find(np) != all_nodes.end()) {
                    new_siblings.push_back(np);
                }

                // Collect parents of affected siblings
                for (auto snode : s) {
                    if (snode != nullptr && all_nodes.find(snode) != all_nodes.end()
                        && snode->get_parent() != nullptr
                        && all_nodes.find(snode->get_parent()) != all_nodes.end()) {
                        p.insert(snode->get_parent());
                    }
                }

                // Adjust MBR and LHV for all affected parents
                for (auto pnode : p) {
                    if (pnode != nullptr && all_nodes.find(pnode) != all_nodes.end()
                        && !pnode->get_entries().empty()) {
                        pnode->adjust_lhv();
                        pnode->adjust_mbr();
                        // CRITICAL: Validate child chains after adjustments
                        if (!pnode->is_leaf()) {
                            validate_and_fix_child_chain(pnode);
                        }
                    }
                }

                node = np;
                del_node = dpparent;
                s.clear();
                p.clear();
                s.insert(new_siblings.begin(), new_siblings.end());
                new_siblings.clear();
            }
        }
    }

    Node<T>* exactSearch(Node<T>* subtree, const Rectangle& rect) {
        if (!subtree || all_nodes.find(subtree) == all_nodes.end()) {
            return nullptr;
        }

        if (subtree->is_leaf()) {
            for (auto entry : subtree->get_entries()) {
                if (entry != nullptr && rect == entry->get_mbr()) {
                    return subtree;
                }
            }
        } else {
            for (auto entry : subtree->get_entries()) {
                if (entry != nullptr && entry->get_mbr().contains(rect)) {
                    auto inner = dynamic_cast<InnerNode<T>*>(entry);
                    if (inner && inner->get_node() != nullptr) {
                        auto res = exactSearch(inner->get_node(), rect);
                        if (res != nullptr) {
                            return res;
                        }
                    }
                }
            }
        }

        return nullptr;
    }

    std::deque<LeafEntry<T>*> _search(Node<T>* subtree, const Rectangle& rect) {
        std::deque<LeafEntry<T>*> result;

        if (subtree == nullptr || all_nodes.find(subtree) == all_nodes.end()) {
            return result;
        }

        if (subtree->is_leaf()) {
            for (auto entry : subtree->get_entries()) {
                if (entry != nullptr && entry->get_mbr().intersects(rect)) {
                    result.push_back(dynamic_cast<LeafEntry<T>*>(entry));
                }
            }
        } else {
            for (auto entry : subtree->get_entries()) {
                if (entry != nullptr && entry->get_mbr().intersects(rect)) {
                    auto inner = dynamic_cast<InnerNode<T>*>(entry);
                    if (inner && inner->get_node() != nullptr) {
                        auto aux = _search(inner->get_node(), rect);
                        result.insert(result.end(), aux.begin(), aux.end());
                    }
                }
            }
        }

        return result;
    }
};

}  // namespace hilbert