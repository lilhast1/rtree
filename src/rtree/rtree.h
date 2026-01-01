#pragma once
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <any>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <stack>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace Gutman {

static int optimization_counter = 0;
inline bool equal(double x, double y, double eps = 1e-7) {

    return std::fabs(x - y) <= eps * (std::fabs(x) + std::fabs(y));
}

struct Rectangle {
    std::vector<double> min;
    std::vector<double> max;

    Rectangle(const std::vector<double>& min, const std::vector<double>& max)
        : min(min), max(max) {}
    [[nodiscard]] double area() const {
        double a = 1;
        for (int i = 0; i < min.size(); i++) {
            a *= std::fabs(min[i] - max[i]);
        }
        return a;
    }

    [[nodiscard]] double enlargement_needed(const Rectangle& b) const {
        auto mbr = calc_mbr(*this, b);
        return mbr.area() - area();
    }

    [[nodiscard]] bool contains(const Rectangle& a) const {
        bool flag = true;
        for (int i = 0; i < min.size(); i++) {
            flag = flag && min[i] < a.min[i] && a.max[i] < max[i];
            if (!flag)
                break;
        }
        return flag;
    }

    template <typename Itr>
    static Rectangle calc_mbr(Itr p, Itr q) {
        if (q == p)
            throw std::range_error("Calculating the MBR on an empty range");
        auto t = p++;
        if (q == p)
            return (*t)->mbr;
        auto n = p;
        auto agg = calc_mbr((*n)->mbr, (*t)->mbr);
        while (n != q) {
            agg = calc_mbr(agg, (*n)->mbr);
            n++;
        }
        return agg;
    }

    static Rectangle calc_mbr(const Rectangle& a, const Rectangle& b) {
        auto c = a;
        std::transform(a.min.begin(), a.min.end(), b.min.begin(), c.min.begin(),
                       [](double a, double b) { return std::min(a, b); });
        std::transform(a.max.begin(), a.max.end(), b.max.begin(), c.max.begin(),
                       [](double a, double b) { return std::max(a, b); });
        return c;
    }

    static double calc_mbr_area(const Rectangle& a, const Rectangle& b) {
        return calc_mbr(a, b).area();
    }

    static bool vec_equal(const std::vector<double>& x, const std::vector<double>& y) {
        bool f = true;
        for (int i = 0; i < x.size(); i++) {
            f = f && Gutman::equal(x[i], y[i]);
            if (!f)
                break;
        }
        return f;
    }

    static bool equal(const Rectangle& a, const Rectangle& b) {
        return vec_equal(a.min, b.min) && vec_equal(a.max, b.max);
    }

    static bool overlap(const Rectangle& a, const Rectangle& b) {
        bool f = true;
        for (int i = 0; i < a.min.size(); i++) {
            f = f && a.max[i] >= b.min[i] && a.min[i] <= b.max[i];
            if (!f)
                break;
        }
        return f;
    }
};

template <typename T>
struct Node {
    static int live_nodes;
    bool is_leaf;
    Node* parent;
    std::vector<Node*> children;
    std::vector<std::pair<T*, Rectangle>> elems;
    Rectangle mbr;

    [[nodiscard]] int count() const { return is_leaf ? elems.size() : children.size(); }

    Node(bool is_leaf, Rectangle mbr) : is_leaf(is_leaf), mbr(std::move(mbr)), parent(nullptr) {
        live_nodes++;
    }
    ~Node() {
        live_nodes--;
        for (auto child : children) delete child;
    }

    void update_mbr() {
        if (is_leaf && elems.size()) {
            auto mbr = elems[0].second;
            for (auto& entry : elems) {
                mbr = Rectangle::calc_mbr(mbr, entry.second);
            }
            this->mbr = mbr;
        } else if (is_leaf && children.size()) {
            auto mbr = children[0]->mbr;
            for (auto node : children) {
                mbr = Rectangle::calc_mbr(mbr, node->mbr);
            }
            this->mbr = mbr;
        }
    }
};
template <typename T>
int Node<T>::live_nodes = 0;
template <typename T>
class RTree {
    int m, M;
    Node<T>* root;
    size_t size;

   public:
    RTree(int m, int M) : root(nullptr), m(m), M(M), size(0) {}
    ~RTree() { delete root; }

    std::vector<T*> search(const Rectangle& search_rect) const {
        std::vector<T*> result;
        _impl_search(search_rect, result, root);
        return result;
    }

    void insert(const Rectangle& mbr, T* elem) {
        if (!root) {
            root = new Node<T>(true, mbr);
            root->elems.push_back({elem, mbr});
            size++;
            return;
        }
        Node<T>* leaf = choose_leaf(mbr, root);
        Node<T>* ll = nullptr;

        if (leaf->count() < M) {
            leaf->elems.push_back({elem, mbr});
        } else {
            // invoke split to get L and LL containing current entry E and all previous leaf entries
            leaf->elems.push_back({elem, mbr});
            ll = split(leaf);
        }

        adjust_tree(leaf, ll);
        size++;
        // if root is split grow the tree taller
    }

    void remove(const Rectangle& r) {
        if (!root)
            return;

        // Find the leaf containing this rectangle
        Node<T>* leaf = find_leaf(r, root);
        if (!leaf)
            return;  // Rectangle not found

        // Remove the entry from the leaf
        auto it = std::find_if(leaf->elems.begin(), leaf->elems.end(),
                               [&r](const std::pair<T*, Rectangle>& elem) {
                                   return Rectangle::equal(elem.second, r);
                               });

        if (it != leaf->elems.end()) {
            leaf->elems.erase(it);
            size--;

            // Condense the tree
            leaf->update_mbr();
            adjust_tree(leaf, nullptr);
            _condense_tree(leaf);

            // condense_tree(leaf);
            // // If root has only one child and is not a leaf, make that child the new root
            // if (root && !root->is_leaf && root->count() == 1) {
            //     Node<T>* old_root = root;
            //     root = root->children[0];
            //     root->parent = nullptr;
            //     for (auto& child : old_root->children) child = nullptr;
            //     old_root->children.clear();  // Prevent deletion of the new root
            //     delete old_root;
            // }

            // // If root is a leaf and empty, set root to nullptr
            if (root && root->is_leaf && root->count() == 0) {
                delete root;
                root = nullptr;
            }
        }
    }

    void update(const Rectangle& current, Rectangle& desired, T* new_elem) {
        remove(current);
        insert(desired, new_elem);
    }

   private:
    struct EntryWrapper {
        int index;
        Rectangle rect;
    };

    void condense_tree(Node<T>* l) {
        Node<T>* n = l;
        std::vector<Node<T>*> internal_orphans;
        std::vector<std::pair<T*, Rectangle>> leaf_entries_to_reinsert;

        // DEBUG: Track loop iterations to prevent infinite loops
        int loop_safety = 0;

        while (n != root) {
            if (loop_safety++ > 1000) {
                std::cerr << "CRITICAL ERROR: Infinite loop detected in condense_tree!\n";
                break;
            }

            Node<T>* p = n->parent;

            // Safety Check: If parent is null but n is not root, tree is corrupt
            if (!p) {
                // If we hit a detached node, stop climbing
                break;
            }

            if (n->count() < m) {
                // Remove n from parent's children list
                auto it = std::find(p->children.begin(), p->children.end(), n);
                if (it != p->children.end()) {
                    p->children.erase(it);
                }

                if (n->is_leaf) {
                    // Leaf: destroy and save items
                    for (auto& entry : n->elems) leaf_entries_to_reinsert.push_back(entry);
                    n->elems.clear();
                    delete n;
                } else {
                    // Internal: Optimization Trigger
                    Gutman::optimization_counter++;

                    // DEBUG PRINT to prove it works
                    // std::cout << "[OPT] Detaching internal node height " << get_heigth(n) <<
                    // "\n";

                    n->parent = nullptr;
                    internal_orphans.push_back(n);
                }
            } else {
                update_mbr(n);
            }
            n = p;
        }

        // ... (Rest of your function stays the same) ...
        update_mbr(root);

        if (!root->is_leaf && root->children.empty()) {
            root->is_leaf = true;
        }
        if (!root->is_leaf && root->children.size() == 1) {
            Node<T>* new_root = root->children[0];
            root->children.clear();
            delete root;
            root = new_root;
            root->parent = nullptr;
        }

        for (const auto& entry : leaf_entries_to_reinsert) insert(entry.second, entry.first);
        for (Node<T>* subtree : internal_orphans) insert_subtree(subtree);
    }

    void _impl_search(const Rectangle& s, std::vector<T*>& result, Node<T>* t = nullptr) const {
        if (t == nullptr)
            t = root;
        if (t == nullptr)
            return;

        if (t->is_leaf) {
            for (auto& elem_rec : t->elems) {
                if (Rectangle::overlap(elem_rec.second, s))
                    result.push_back(elem_rec.first);
            }
            return;
        }

        for (auto node : t->children) {
            if (Rectangle::overlap(node->mbr, s)) {
                std::vector<T*> r;
                _impl_search(s, r, node);
                std::copy(r.begin(), r.end(), std::back_inserter(result));
            }
        }
    }
    Node<T>* choose_leaf(Rectangle s, Node<T>* n = nullptr) {
        if (!n)
            n = root;
        if (!n)
            return nullptr;

        if (n->is_leaf)
            return n;

        double min_area = std::numeric_limits<double>::max();
        double min_enlarge = min_area;

        auto leaf = n;

        for (auto node : n->children) {
            const double e = node->mbr.enlargement_needed(s);
            if (!equal(e, min_enlarge) && e < min_enlarge) {
                min_enlarge = e;
                min_area = node->mbr.area();
                leaf = node;
            } else if (equal(e, min_enlarge) && node->mbr.area() < min_area) {
                min_enlarge = e;
                min_area = node->mbr.area();
                leaf = node;
            }
        }
        return choose_leaf(s, leaf);
    }

    void adjust_tree(Node<T>* l, Node<T>* ll) {
        if (l == nullptr)
            return;

        // CRITICAL FIX: Always update the MBR of the node we're adjusting
        if (l->count() > 0) {
            update_mbr(l);
        }

        auto p = l->parent;
        if (p == nullptr && ll == nullptr) {
            if (l->count() > M) {
                ll = split(l);
                adjust_tree(l, ll);
            }
            return;
        } else if (p == nullptr && ll != nullptr) {  // root was split
            auto rect = Rectangle::calc_mbr(ll->mbr, l->mbr);
            root = new Node<T>(false, rect);
            root->children = std::vector<Node<T>*>{l, ll};
            l->parent = ll->parent = root;
            return;
        } else if (p != nullptr && ll == nullptr) {  // just adjust the mbr
            update_mbr(p);
            adjust_tree(p, nullptr);
        } else {  // split if needed
            p->children.push_back(ll);
            ll->parent = p;
            Node<T>* pp = nullptr;
            if (p->count() > M) {
                pp = split(p);
            }
            update_mbr(p);
            if (ll)
                update_mbr(ll);
            adjust_tree(p, pp);
        }
    }

    Node<T>* split(Node<T>* t) {
        // apply pickseeds
        std::vector<EntryWrapper> entries;
        std::vector<bool> assigned(t->count(), false);
        if (t->is_leaf) {
            for (int i = 0; i < t->count(); i++)
                entries.push_back(EntryWrapper{i, t->elems[i].second});
        } else {
            for (int i = 0; i < t->count(); i++)
                entries.push_back(EntryWrapper{i, t->children[i]->mbr});
        }

        auto seeds = pick_seeds(entries);
        assigned[seeds.first] = assigned[seeds.second] = true;

        int n = t->count();

        auto mbr1 = entries[seeds.first].rect;
        auto mbr2 = entries[seeds.second].rect;

        std::vector<int> g1{seeds.first}, g2{seeds.second};

        for (int i = 0; i < n; i++) {
            int next = pick_next(entries, assigned, mbr1, mbr2);
            if (next < 0)
                continue;
            assigned[next] = true;
            auto c1 = Rectangle::calc_mbr(mbr1, entries[next].rect);
            auto c2 = Rectangle::calc_mbr(mbr2, entries[next].rect);
            if (c1.area() < c2.area()) {
                mbr1 = c1;
                g1.push_back(next);
            } else {
                mbr2 = c2;
                g2.push_back(next);
            }
        }

        // Enforce minimum entries per node
        if (g1.size() < m || g2.size() < m) {
            auto& big = g1.size() > g2.size() ? g1 : g2;
            auto& small = g1.size() < g2.size() ? g1 : g2;

            while (small.size() < m) {
                small.push_back(big.back());
                big.pop_back();
            }

            // CRITICAL FIX: Recalculate MBRs after redistribution
            mbr1 = entries[g1[0]].rect;
            for (size_t i = 1; i < g1.size(); i++) {
                mbr1 = Rectangle::calc_mbr(mbr1, entries[g1[i]].rect);
            }

            mbr2 = entries[g2[0]].rect;
            for (size_t i = 1; i < g2.size(); i++) {
                mbr2 = Rectangle::calc_mbr(mbr2, entries[g2[i]].rect);
            }
        }

        auto tt = new Node<T>{t->is_leaf, mbr2};
        tt->parent = t->parent;
        t->mbr = mbr1;

        if (t->is_leaf) {
            std::vector<std::pair<T*, Rectangle>> elems1;
            std::vector<std::pair<T*, Rectangle>> elems2;

            for (auto i : g1) elems1.push_back(t->elems[i]);
            for (auto i : g2) elems2.push_back(t->elems[i]);

            for (auto& e : t->elems) {
                e.first = nullptr;
            }

            t->elems = elems1;
            tt->elems = elems2;
        } else {
            std::vector<Node<T>*> elems1;
            std::vector<Node<T>*> elems2;

            for (auto i : g1) elems1.push_back(t->children[i]);
            for (auto i : g2) elems2.push_back(t->children[i]);

            for (auto& e : t->children) {
                e = nullptr;
            }

            t->children = elems1;
            tt->children = elems2;

            // Update parent pointers
            for (auto child : t->children) {
                child->parent = t;
            }
            for (auto child : tt->children) {
                child->parent = tt;
            }
        }

        return tt;
    }

    Node<T>* find_leaf(const Rectangle& r, Node<T>* t = nullptr) {
        if (t == nullptr)
            t = root;
        if (t == nullptr)
            return nullptr;

        if (t->is_leaf) {
            for (auto& elem_rec : t->elems) {
                if (Rectangle::equal(elem_rec.second, r))
                    return t;
            }
        } else {
            for (auto node : t->children) {
                if (Rectangle::overlap(node->mbr, r)) {
                    auto leaf = find_leaf(r, node);
                    if (leaf)
                        return leaf;
                }
            }
        }

        return nullptr;
    }

    std::pair<int, int> pick_seeds(const std::vector<EntryWrapper>& entries) {
        double max_waste = -1;
        int n = entries.size();
        std::pair<int, int> seeds{-1, -1};
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                if (i == j)
                    continue;
                auto b = Rectangle::calc_mbr(entries[i].rect, entries[j].rect);
                double d = b.area() - entries[i].rect.area() - entries[j].rect.area();
                if (std::fabs(d) > max_waste) {
                    max_waste = d;
                    seeds.first = i;
                    seeds.second = j;
                }
            }
        }
        return seeds;
    }

    int pick_next(const std::vector<struct EntryWrapper>& entries,
                  const std::vector<bool>& assigned, const Rectangle& mbr1, const Rectangle& mbr2) {
        int n = entries.size();
        double max_diff = -100000;
        int max_i = -1;
        for (int i = 0; i < n; i++) {
            if (assigned[i])
                continue;
            auto d1 = mbr1.enlargement_needed(entries[i].rect);
            auto d2 = mbr2.enlargement_needed(entries[i].rect);
            if (std::fabs(d1 - d2) > max_diff) {
                max_i = i;
                max_diff = std::fabs(d1 - d2);
            }
        }
        return max_i;
    }

    void update_mbr(Node<T>* n) {
        if (!n || n->count() == 0)
            return;

        if (n->is_leaf) {
            // Calculate MBR from elements
            n->mbr = n->elems[0].second;
            for (size_t i = 1; i < n->elems.size(); i++) {
                n->mbr = Rectangle::calc_mbr(n->mbr, n->elems[i].second);
            }
        } else {
            // Calculate MBR from children
            n->mbr = n->children[0]->mbr;
            for (size_t i = 1; i < n->children.size(); i++) {
                n->mbr = Rectangle::calc_mbr(n->mbr, n->children[i]->mbr);
            }
        }
    }

    void collect_data_from_subtree(Node<T>* node, std::vector<std::pair<T*, Rectangle>>& data) {
        if (node->is_leaf) {
            for (auto& elem : node->elems) {
                data.push_back(elem);
            }
            for (auto& elem : node->elems) {
                elem.first = nullptr;
            }
            // Clear to prevent double-delete
            node->elems.clear();
        } else {
            for (auto child : node->children) {
                collect_data_from_subtree(child, data);
            }
            // Clear children to prevent destructor from deleting them
            // (we already processed them recursively)
            for (auto& child : node->children) child = nullptr;
            node->children.clear();  // OVO NIJE TACNO .CLEAR zove destruktore
        }
    }

    void _condense_tree(Node<T>* l) {
        Node<T>* n = l;

        // Store orphans.
        // Items are for leaf underflows.
        // Nodes are for internal node underflows (The Optimization).
        std::vector<std::pair<T*, Rectangle>> leaf_orphans;
        std::vector<Node<T>*> subtree_orphans;

        // 1. ASCEND AND COLLECT ORPHANS
        while (n != root) {
            Node<T>* p = n->parent;

            if (n->count() < m) {
                // Detach n from parent p
                auto it = std::find(p->children.begin(), p->children.end(), n);
                if (it != p->children.end()) {
                    p->children.erase(it);
                }

                if (n->is_leaf) {
                    // If leaf, save the data items
                    for (auto& entry : n->elems) {
                        leaf_orphans.push_back(entry);
                    }
                    n->elems.clear();  // Clear so delete doesn't touch T*
                } else {
                    Gutman::optimization_counter++;
                    // --- OPTIMIZATION STARTS HERE ---
                    // If internal, SAVE THE CHILDREN AS SUBTREES.
                    // Do not destroy them.
                    for (auto* child : n->children) {
                        // Crucial: The child is now detached, waiting for a new parent
                        child->parent = nullptr;
                        subtree_orphans.push_back(child);
                    }
                    n->children.clear();  // Clear so delete doesn't recursive delete children
                    // --- OPTIMIZATION ENDS HERE ---
                }

                // Destroy the empty shell of node n
                delete n;
            } else {
                // Node is healthy, just needs MBR adjustment
                n->update_mbr();
            }
            n = p;
        }

        // 2. ADJUST ROOT MBR (Since children might have changed)
        root->update_mbr();

        // 3. HANDLE ROOT UNDERFLOW (Height Reduction)
        // If root is internal and has 0 children (tree empty) or 1 child (useless level)
        if (!root->is_leaf && root->children.empty()) {
            // Tree became empty (or just contained the nodes we deleted)
            root->is_leaf = true;
        } else if (!root->is_leaf && root->children.size() == 1) {
            // Shrink tree height
            Node<T>* new_root = root->children[0];
            root->children.clear();  // Detach to prevent deletion
            delete root;
            root = new_root;
            root->parent = nullptr;
        }

        // 4. REINSERT LEAF ITEMS (Standard Insert)
        for (const auto& entry : leaf_orphans) {
            insert(entry.second, entry.first);
        }

        // 5. REINSERT SUBTREES (Grafting)
        // We reinsert entire branches instead of breaking them down
        for (Node<T>* subtree : subtree_orphans) {
            insert_subtree(subtree);
        }
    }
    void insert_subtree(Node<T>* subtree) {
        // Calculate the height of the subtree we are inserting
        int subtree_height = get_height(subtree);
        int root_height = get_height(root);

        // CASE 1: The subtree is taller than (or equal to) the current root.
        // We must grow the tree upwards to accommodate this large orphan.
        while (subtree_height >= root_height) {
            Node<T>* new_root = new Node<T>(false, root->mbr);
            new_root->children.push_back(root);
            root->parent = new_root;
            root = new_root;
            root->update_mbr();
            root_height++;
        }

        // CASE 2: Find a suitable parent at (subtree_height + 1)
        // We use a helper similar to choose_leaf, but it stops at a specific level
        Node<T>* target_parent = choose_node_at_level(root, subtree->mbr, subtree_height + 1);

        // Attach the subtree
        target_parent->children.push_back(subtree);
        subtree->parent = target_parent;

        // Propagate MBR changes upward
        target_parent->update_mbr();

        // Check for Overflow (Split) on the target parent
        if (target_parent->count() > M) {
            Node<T>* split_node = split(target_parent);
            adjust_tree(target_parent, split_node);
        } else {
            // If no split, we still need to propagate MBR changes up to root
            // (adjust_tree handles this, or we do it manually)
            adjust_tree(target_parent, nullptr);
        }
    }
    void insert_node_at_level(Node<T>* subtree, int level) {
        // 1. Choose Subtree (Find the best parent for this node)
        // We want a node at (level + 1) to be the parent.
        Node<T>* target_parent = choose_subtree(root, subtree->mbr, level);

        // 2. Add child
        target_parent->children.push_back(subtree);
        subtree->parent = target_parent;
        target_parent->update_mbr();

        // 3. Handle Overflow (Split) if necessary
        // This propagates up just like a normal insert
        if (target_parent->children.size() > M) {
            auto ll = split(target_parent);
            adjust_tree(target_parent, ll);
        }
    }

    Node<T>* choose_subtree(Node<T>* node, const Rectangle& rect, int insertion_level) {
        // Base Case: If we are at the level immediately above the insertion level,
        // this node is the candidate parent.
        // Example: Inserting a Leaf (level 0). We need a parent at Level 1 (Internal).
        // Note: You need a way to determine node height/level.
        // If you don't store 'level' in Node, you calculate it based on distance to leaves.

        int current_level = get_level(node);  // See helper below
        if (current_level == insertion_level + 1) {
            return node;
        }

        // Standard R-Tree logic: Find child requiring least enlargement
        Node<T>* best_child = nullptr;
        double min_enlargement = std::numeric_limits<double>::max();
        double best_area = std::numeric_limits<double>::max();

        for (Node<T>* child : node->children) {
            double area = child->mbr.area();
            double enlargement = Rectangle::calc_mbr(child->mbr, rect).area() - area;

            if (enlargement < min_enlargement) {
                min_enlargement = enlargement;
                best_area = area;
                best_child = child;
            } else if (enlargement == min_enlargement) {
                if (area < best_area) {
                    best_area = area;
                    best_child = child;
                }
            }
        }

        // Recurse
        return choose_subtree(best_child, rect, insertion_level);
    }

    int get_level(Node<T>* node) {
        int lvl = 0;
        while (!node->is_leaf) {
            // Go down the first child until we hit a leaf
            if (node->children.empty())
                break;  // Should not happen in valid tree
            node = node->children[0];
            lvl++;
        }
        return lvl;
    }

    int get_height(Node<T>* n) {
        int h = 0;
        Node<T>* curr = n;
        while (!curr->is_leaf) {
            if (curr->children.empty())
                break;
            curr = curr->children[0];
            h++;
        }
        return h;
    }

    Node<T>* choose_node_at_level(Node<T>* node, const Rectangle& mbr, int target_height) {
        int current_height = get_height(node);

        // Base case: We found the level
        if (current_height == target_height) {
            return node;
        }

        // Traverse down finding minimum enlargement (Same logic as ChooseLeaf)
        Node<T>* best_child = nullptr;
        double min_enlargement = std::numeric_limits<double>::max();
        double best_area = std::numeric_limits<double>::max();

        for (auto child : node->children) {
            double area = child->mbr.area();
            double enlargement = Rectangle::calc_mbr(child->mbr, mbr).area() - area;

            if (enlargement < min_enlargement) {
                min_enlargement = enlargement;
                best_area = area;
                best_child = child;
            } else if (equal(enlargement, min_enlargement)) {
                if (area < best_area) {
                    best_area = area;
                    best_child = child;
                }
            }
        }

        // Fallback
        if (!best_child && !node->children.empty())
            best_child = node->children[0];

        if (best_child) {
            return choose_node_at_level(best_child, mbr, target_height);
        }

        return node;
    }

    // Function to re-insert a subtree (internal node)
    void insert_internal(Node<T>* subtree) {
        int subtree_height = get_height(subtree);
        int root_height = get_height(root);

        // CASE: Tree is too short.
        // If the orphan is as tall as the root, we must create a new root
        // to hold both the old root and the orphan.
        if (subtree_height >= root_height) {
            Node<T>* new_root = new Node<T>(false, root->mbr);  // Internal
            new_root->children.push_back(root);
            root->parent = new_root;

            // The subtree also goes into this new root (or its appropriate level,
            // but if heights match, it goes here).
            // However, to keep logic uniform, we just update root and let choose_subtree work.
            root = new_root;
            root->update_mbr();
        }

        // Find the correct parent (guaranteed to exist now)
        Node<T>* target_parent = choose_subtree_at_level(root, subtree->mbr, subtree_height);

        // Link
        target_parent->children.push_back(subtree);
        subtree->parent = target_parent;

        // Update MBRs
        target_parent->update_mbr();

        // Check Overflow
        if (target_parent->children.size() > M) {
            // Assuming you have a standard split function
            auto ll = split(target_parent);
            adjust_tree(target_parent, ll);
        } else {
            // Propagate MBR changes up
            Node<T>* p = target_parent->parent;
            while (p) {
                p->update_mbr();
                p = p->parent;
            }
        }
    }

    void reinsert_subtree(Node<T>* node) { return; }
};
}  // namespace Gutman