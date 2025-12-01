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
        if (!root)
            return result;

        result.reserve(100);  // Pre-allocate some space

        // Use iterative approach with stack instead of recursion
        std::vector<Node<T>*> stack;
        stack.reserve(64);  // Typical tree depth
        stack.push_back(root);

        while (!stack.empty()) {
            Node<T>* node = stack.back();
            stack.pop_back();

            if (node->is_leaf) {
                for (auto& elem_rec : node->elems) {
                    if (Rectangle::overlap(elem_rec.second, search_rect)) {
                        result.push_back(elem_rec.first);
                    }
                }
            } else {
                for (auto child : node->children) {
                    if (Rectangle::overlap(child->mbr, search_rect)) {
                        stack.push_back(child);
                    }
                }
            }
        }

        return result;
    }

    void insert(const Rectangle& mbr, T* elem) {
        if (!root) {
            root = new Node<T>(true, mbr);
            root->elems.reserve(M + 1);  // Pre-allocate
            root->elems.push_back({elem, mbr});
            size++;
            return;
        }
        Node<T>* leaf = choose_leaf(mbr, root);
        Node<T>* ll = nullptr;

        if (leaf->count() < M) {
            if (leaf->elems.capacity() == 0) {
                leaf->elems.reserve(M + 1);
            }
            leaf->elems.push_back({elem, mbr});
        } else {
            if (leaf->elems.capacity() <= M) {
                leaf->elems.reserve(M + 1);
            }
            leaf->elems.push_back({elem, mbr});
            ll = split(leaf);
        }

        adjust_tree(leaf, ll);
        size++;
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

    void condense_tree(Node<T>* l) {
        Node<T>* n = l;
        std::vector<Node<T>*> q;

        while (n != root) {
            Node<T>* p = n->parent;

            if (n->count() < m) {
                auto it = std::find(p->children.begin(), p->children.end(), n);
                if (it != p->children.end()) {
                    p->children.erase(it);
                }
                n->parent = nullptr;
                q.push_back(n);
            } else {
                update_mbr(n);
            }

            n = p;
        }

        if (root && root->count() > 0) {
            update_mbr(root);
        }

        // Collect all entries
        std::vector<std::pair<T*, Rectangle>> leaf_entries;
        leaf_entries.reserve(q.size() * M);  // Pre-allocate

        for (Node<T>* orphan : q) {
            if (orphan->is_leaf) {
                for (auto& elem : orphan->elems) {
                    leaf_entries.push_back(elem);
                }
                orphan->elems.clear();
                delete orphan;
            } else {
                collect_data_from_subtree(orphan, leaf_entries);
                delete orphan;
            }
        }

        // Batch reinsert - this is critical for performance
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
        } else if (p == nullptr && ll != nullptr) {
            auto rect = Rectangle::calc_mbr(ll->mbr, l->mbr);
            root = new Node<T>(false, rect);
            root->children.reserve(M + 1);  // Pre-allocate
            root->children = {l, ll};
            l->parent = ll->parent = root;
            return;
        } else if (p != nullptr && ll == nullptr) {
            update_mbr(p);
            adjust_tree(p, nullptr);
        } else {
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
        int remaining = n - 2;

        while (remaining > 0) {
            // Check if we must assign all remaining to one group
            if (g1.size() + remaining == m) {
                // Must put all remaining in g1
                for (int i = 0; i < n; i++) {
                    if (!assigned[i]) {
                        assigned[i] = true;
                        g1.push_back(i);
                        mbr1 = Rectangle::calc_mbr(mbr1, entries[i].rect);
                        remaining--;
                    }
                }
                break;
            } else if (g2.size() + remaining == m) {
                // Must put all remaining in g2
                for (int i = 0; i < n; i++) {
                    if (!assigned[i]) {
                        assigned[i] = true;
                        g2.push_back(i);
                        mbr2 = Rectangle::calc_mbr(mbr2, entries[i].rect);
                        remaining--;
                    }
                }
                break;
            }

            int next = pick_next(entries, assigned, mbr1, mbr2, remaining, g1.size(), g2.size());
            if (next < 0)
                break;

            assigned[next] = true;
            remaining--;

            auto c1 = Rectangle::calc_mbr(mbr1, entries[next].rect);
            auto c2 = Rectangle::calc_mbr(mbr2, entries[next].rect);

            double area_increase1 = c1.area() - mbr1.area();
            double area_increase2 = c2.area() - mbr2.area();

            if (area_increase1 < area_increase2) {
                mbr1 = c1;
                g1.push_back(next);
            } else if (area_increase2 < area_increase1) {
                mbr2 = c2;
                g2.push_back(next);
            } else {
                // Tie: prefer smaller area
                if (mbr1.area() < mbr2.area()) {
                    mbr1 = c1;
                    g1.push_back(next);
                } else {
                    mbr2 = c2;
                    g2.push_back(next);
                }
            }
        }

        // Safety check - ensure minimum node size
        if (g1.size() < m || g2.size() < m) {
            auto& big = g1.size() > g2.size() ? g1 : g2;
            auto& small = g1.size() < g2.size() ? g1 : g2;

            while (small.size() < m && big.size() > m) {
                small.push_back(big.back());
                big.pop_back();
            }

            // Recalculate MBRs
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
            std::vector<std::pair<T*, Rectangle>> elems1, elems2;
            elems1.reserve(g1.size());
            elems2.reserve(g2.size());

            for (auto i : g1) elems1.push_back(t->elems[i]);
            for (auto i : g2) elems2.push_back(t->elems[i]);

            t->elems = std::move(elems1);
            tt->elems = std::move(elems2);
        } else {
            std::vector<Node<T>*> children1, children2;
            children1.reserve(g1.size());
            children2.reserve(g2.size());

            for (auto i : g1) children1.push_back(t->children[i]);
            for (auto i : g2) children2.push_back(t->children[i]);

            t->children = std::move(children1);
            tt->children = std::move(children2);

            for (auto child : t->children) child->parent = t;
            for (auto child : tt->children) child->parent = tt;
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
        int n = entries.size();
        if (n < 2)
            return {0, 0};

        // Linear time pick_seeds: find the pair with maximum separation along each dimension
        int dimensions = entries[0].rect.min.size();
        double max_separation = -1;
        std::pair<int, int> seeds{0, 1};

        for (int dim = 0; dim < dimensions; dim++) {
            // Find entries with lowest and highest values in this dimension
            int lowest_idx = 0, highest_idx = 0;
            double lowest = entries[0].rect.min[dim];
            double highest = entries[0].rect.max[dim];

            for (int i = 1; i < n; i++) {
                if (entries[i].rect.min[dim] < lowest) {
                    lowest = entries[i].rect.min[dim];
                    lowest_idx = i;
                }
                if (entries[i].rect.max[dim] > highest) {
                    highest = entries[i].rect.max[dim];
                    highest_idx = i;
                }
            }

            if (lowest_idx != highest_idx) {
                double separation = highest - lowest;
                if (separation > max_separation) {
                    max_separation = separation;
                    seeds = {lowest_idx, highest_idx};
                }
            }
        }

        return seeds;
    }

    int pick_next(const std::vector<struct EntryWrapper>& entries,
                  const std::vector<bool>& assigned, const Rectangle& mbr1, const Rectangle& mbr2,
                  int remaining, int g1_size, int g2_size) {
        int n = entries.size();

        // Early termination: if we must put all remaining in one group, return any
        if (remaining == m - g1_size || remaining == m - g2_size) {
            for (int i = 0; i < n; i++) {
                if (!assigned[i])
                    return i;
            }
        }

        double max_diff = -1;
        int max_i = -1;
        for (int i = 0; i < n; i++) {
            if (assigned[i])
                continue;

            auto d1 = mbr1.enlargement_needed(entries[i].rect);
            auto d2 = mbr2.enlargement_needed(entries[i].rect);
            double diff = std::fabs(d1 - d2);

            if (diff > max_diff) {
                max_i = i;
                max_diff = diff;
            }
        }
        return max_i;
    }

    void reinsert_subtree(Node<T>* node) { return; }
};
}  // namespace Gutman