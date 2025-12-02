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

bool equal(double x, double y, double eps = 1e-7) {
    return std::fabs(x - y) <= eps * (std::fabs(x) + std::fabs(y));
}

struct Rectangle {
    std::vector<double> min;
    std::vector<double> max;

    Rectangle(const std::vector<double>& min, const std::vector<double>& max)
        : min(min), max(max) {}
    double area() const {
        double a = 1;
        for (int i = 0; i < min.size(); i++) {
            a *= std::fabs(min[i] - max[i]);
        }
        return a;
    }

    double enlargement_needed(const Rectangle& b) const {
        auto mbr = calc_mbr(*this, b);
        return mbr.area() - area();
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
    bool is_leaf;
    Node* parent;
    std::vector<Node*> children;
    std::vector<std::pair<T*, Rectangle>> elems;
    Rectangle mbr;

    int count() const { return is_leaf ? elems.size() : children.size(); }

    Node(bool is_leaf, Rectangle mbr) : is_leaf(is_leaf), mbr(mbr), parent(nullptr) {}
    ~Node() {
        for (auto child : children) delete child;
    }
};

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

    void remove(Rectangle r) {
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
            condense_tree(leaf);

            // If root has only one child and is not a leaf, make that child the new root
            if (root && !root->is_leaf && root->count() == 1) {
                Node<T>* old_root = root;
                root = root->children[0];
                root->parent = nullptr;
                old_root->children.clear();  // Prevent deletion of the new root
                delete old_root;
            }

            // If root is a leaf and empty, set root to nullptr
            if (root && root->is_leaf && root->count() == 0) {
                delete root;
                root = nullptr;
            }
        }
    }

   private:
    struct EntryWrapper {
        int index;
        Rectangle rect;
    };

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
            // Clear to prevent double-delete
            node->elems.clear();
        } else {
            for (auto child : node->children) {
                collect_data_from_subtree(child, data);
            }
            // Clear children to prevent destructor from deleting them
            // (we already processed them recursively)
            node->children.clear();
        }
    }

    void condense_tree(Node<T>* l) {
        Node<T>* n = l;
        std::vector<Node<T>*> q;  // Set of eliminated nodes to reinsert

        while (n != root) {
            Node<T>* p = n->parent;

            if (n->count() < m) {
                // Remove n from parent's children list
                auto it = std::find(p->children.begin(), p->children.end(), n);
                if (it != p->children.end()) {
                    p->children.erase(it);
                }

                // CRITICAL: Set parent to nullptr to mark this node as orphaned
                n->parent = nullptr;

                // Add to elimination list
                q.push_back(n);
            } else {
                // Node is not underflow, just update its MBR
                update_mbr(n);
            }

            // Always move to parent
            n = p;
        }

        // Update root MBR if it still exists and has children
        if (root && root->count() > 0) {
            update_mbr(root);
        }

        // Reinsert all entries from eliminated nodes
        std::vector<std::pair<T*, Rectangle>> leaf_entries;

        for (Node<T>* orphan : q) {
            if (orphan->is_leaf) {
                // Collect leaf entries
                for (auto& elem : orphan->elems) {
                    leaf_entries.push_back(elem);
                }
                // Clear to prevent destructor issues
                orphan->elems.clear();
                delete orphan;
            } else {
                // For non-leaf nodes, collect all leaf entries from subtree
                collect_data_from_subtree(orphan, leaf_entries);
                // Destructor will handle cleanup
                delete orphan;
            }
        }

        // Reinsert all collected entries
        for (const auto& entry : leaf_entries) {
            insert(entry.second, entry.first);
        }
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
            // Clear to prevent double-delete
            node->elems.clear();
        } else {
            for (auto child : node->children) {
                collect_data_from_subtree(child, data);
            }
            // Clear children to prevent destructor from deleting them
            // (we already processed them recursively)
            node->children.clear();
        }
    }

    void reinsert_subtree(Node<T>* node) { return; }
};
}  // namespace Gutman