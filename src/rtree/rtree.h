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

template <typename T>
class RTreeGutman {
   public:
    struct Node;
    struct Block;

    // private:
    Block* root;
    int m;
    int M;

    static double equal(double x, double y, double eps = 1e-7) {
        return std::fabs(x - y) <= eps * (std::fabs(x) + std::fabs(y));
    }

   public:
    RTreeGutman(int m, int M) : root(nullptr), m(m), M(M) {
        if (m > M / 2)
            throw std::length_error(
                "The minimum number of entries in a node must be at most M / 2");
    }
    ~RTreeGutman() { delete root; }
    struct Rectangle {
        std::vector<double> min;
        std::vector<double> max;
        Rectangle(const std::vector<double>& min, const std::vector<double>& max)
            : min(min), max(max) {}
        static bool overlap(const Rectangle& a, const Rectangle& b) {
            bool f = true;
            for (int i = 0; i < a.min.size(); i++) {
                f = f && a.max[i] >= b.min[i] && a.min[i] <= b.max[i];
                if (!f)
                    break;
            }
            return f;
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
        double enlargement_needed(const Rectangle& b) const {
            auto mbr = calc_mbr(*this, b);
            return mbr.area() - area();
        }
        double area() const {
            double a = 1;
            for (int i = 0; i < min.size(); i++) {
                a *= std::fabs(min[i] - max[i]);
            }
            return a;
        }
        static bool vec_equal(const std::vector<double>& x, const std::vector<double>& y) {
            bool f = true;
            for (int i = 0; i < x.size(); i++) {
                f = f && RTreeGutman::equal(x[i], y[i]);
                if (!f)
                    break;
            }
            return f;
        }
        static bool equal(const Rectangle& a, const Rectangle& b) {
            return vec_equal(a.min, b.min) && vec_equal(a.max, b.max);
        }
    };

    struct Node {
        Rectangle mbr;
        Block* children;
        Block* parent;
        T* elem;
        ~Node() { delete children; }
    };

    struct Block {
        bool is_leaf;
        std::vector<Node*> nodes;
        Node* parent;
        int count;
        ~Block() {
            for (auto x : nodes) delete x;
        }
    };

    // search
    std::vector<T*>& search(Rectangle s, std::vector<T*>& result, Block* t = nullptr) {
        if (t == nullptr)
            t = root;
        if (t == nullptr)
            return result;
        if (t->is_leaf) {
            for (int i = 0; i < t->count; i++) {
                if (Rectangle::overlap(t->nodes[i]->mbr, s)) {
                    result.push_back(t->nodes[i]->elem);
                }
            }
            return result;
        }
        for (int i = 0; i < t->count; i++) {
            if (Rectangle::overlap(t->nodes[i]->mbr, s)) {
                std::vector<T*> r;
                search(s, r, t->nodes[i]->children);
                std::copy(r.begin(), r.end(), std::back_inserter(result));
            }
        }
        return result;
    }

    // insert
    void insert(Rectangle r, T* elem = nullptr) {
        if (root == nullptr) {
            root = new Block{
                .is_leaf = true, .nodes = std::vector<Node*>(), .parent = nullptr, .count = 1};
            root->nodes.push_back(
                new Node{.mbr = r, .children = nullptr, .parent = root, .elem = elem});
            return;
        }

        auto leaf = choose_leaf(r);
        leaf->nodes.push_back(
            new Node{.mbr = r, .children = nullptr, .parent = leaf, .elem = elem});
        leaf->count++;
        if (leaf->count > M) {
            split(leaf);
        }
        adjust_tree(leaf);
    }

    // chooseleaf
    Block* choose_leaf(Rectangle s, Block* n = nullptr) {
        // auto n = root;
        if (n == nullptr)
            n = root;

        if (n->is_leaf)
            return n;

        double min_area = std::numeric_limits<double>::max();
        double min_enlarge = min_area;
        auto leaf = n;

        for (int i = 0; i < n->count; i++) {
            auto& r = n->nodes[i]->mbr;
            const double e = r.enlargement_needed(s);
            if (!equal(e, min_enlarge) && e < min_enlarge) {
                min_enlarge = e;
                min_area = r.area();
                leaf = n->nodes[i]->children;
            } else if (equal(e, min_enlarge) && r.area() < min_area) {
                min_enlarge = e;
                min_area = r.area();
                leaf = n->nodes[i]->children;
            }
        }
        return choose_leaf(s, leaf);
    }

    // adjusttree
    void adjust_tree(Block* block) {
        if (block == nullptr)
            return;
        if (block->parent == nullptr)
            return;
        if (block->count == 0)
            return;

        auto mbr = Rectangle::calc_mbr(block->nodes.begin(), block->nodes.end());
        if (Rectangle::equal(block->parent->mbr, mbr)) {
            return;
        }
        block->parent->mbr = mbr;
        if (block->parent->parent) {
            adjust_tree(block->parent->parent);
        }
    }
    // delete
    void remove(Rectangle r) {
        if (root == nullptr)
            return;
        auto block = find_leaf(r, root);
        if (block == nullptr)
            return;
        int i = 0;
        for (i = 0; i < block->count; i++) {
            if (Rectangle::equal(block->nodes[i]->mbr, r))
                break;
        }
        if (i == block->count)
            return;
        auto node = block->nodes.at(i);
        block->nodes.erase(block->nodes.begin() + i);
        block->count--;
        delete node;
        condense_tree(block);
        if (root != nullptr && root->count == 1 && !root->is_leaf) {
            auto d = root;
            root = root->nodes[0]->children;
            if (root)
                root->parent = nullptr;
            d->count = 0;
            d->nodes.clear();
            delete d;
        }
        if (root != nullptr && root->nodes.size() == 0) {
            delete root;
            root = nullptr;
        }
    }

    // findleaf
    Block* find_leaf(Rectangle r, Block* t = nullptr) {
        if (t == nullptr)
            t = root;
        if (t->is_leaf) {
            for (int i = 0; i < t->count; i++) {
                if (Rectangle::equal(t->nodes[i]->mbr, r))
                    return t;
            }
            return nullptr;
        }
        auto p = nullptr;
        for (int i = 0; i < t->count; i++) {
            if (Rectangle::overlap(t->nodes[i]->mbr, r)) {
                auto u = find_leaf(r, t->nodes[i]->children);
                if (u)
                    return u;
            }
        }
        return p;
    }
    // condensetree

    void condense_tree(Block* t) {
        std::stack<Node*> orphans;

        while (t->parent) {
            auto parent = t->parent;
            auto parent_block = parent->parent;
            if (t->count < m) {
                int i = 0;
                for (i = 0; i < parent_block->count; i++) {
                    if (parent_block->nodes[i] == parent) {
                        parent_block->nodes.erase(parent_block->nodes.begin() + i);
                        parent_block->count--;
                        break;
                    }
                }
                for (auto p : t->nodes) orphans.push(p);
                t->nodes.clear();
                t->count = 0;
                parent->children = nullptr;
                delete parent;
                // parent_block->count--;
            } else {
                if (t->count > 0) {
                    parent->mbr = Rectangle::calc_mbr(t->nodes.begin(), t->nodes.end());
                }
            }
            t = parent_block;
        }
        while (!orphans.empty()) {
            auto n = orphans.top();
            orphans.pop();
            reinsert_subtree(n);
            delete n;
        }
    }
    void reinsert_subtree(Node* node) {
        if (node->children == nullptr) {
            if (node->elem) {
                insert(node->mbr, node->elem);
            }
        } else {
            if (node->children->is_leaf) {
                for (auto child : node->children->nodes) {
                    if (child->elem) {
                        insert(child->mbr, child->elem);
                    }
                }
            } else {
                for (auto child : node->children->nodes) {
                    reinsert_subtree(child);
                }
            }
            node->children->nodes.clear();
            delete node->children;
            node->children = nullptr;
        }
    }

    // linearsplit
    // linearpickseeds

    // quadraticsplit
    void split(Block* t) {
        // u sada ima M + 1 elem
        // quadratic split
        double max_mbr = -1;
        Node* p = nullptr;
        Node* q = nullptr;
        for (int i = 0; i < t->count; i++) {
            for (int j = 0; j < t->count; j++) {
                if (i == j)
                    continue;
                double mbr = Rectangle::calc_mbr_area(t->nodes[i]->mbr, t->nodes[j]->mbr);
                if (mbr > max_mbr) {
                    max_mbr = mbr;
                    p = t->nodes[i];
                    q = t->nodes[j];
                }
            }
        }

        Node* right_parent = new Node{.mbr = p->mbr};
        Block* right_block = new Block{
            .is_leaf = t->is_leaf,
            .nodes = std::vector<Node*>{p},
            .count = 1,
        };
        right_parent->children = right_block;
        right_block->parent = right_parent;
        p->parent = right_block;

        Node* left_parent = new Node{.mbr = q->mbr};
        Block* left_block = new Block{
            .is_leaf = t->is_leaf,
            .nodes = std::vector<Node*>{q},
            .count = 1,
        };
        left_parent->children = left_block;
        left_block->parent = left_parent;
        q->parent = left_block;

        for (int i = 0; i < t->count; i++) {
            auto ptr = t->nodes[i];
            if (ptr == p || ptr == q)
                continue;
            const auto la = left_parent->mbr.area();
            const auto ra = right_parent->mbr.area();
            const auto a = ptr->mbr.area();
            if (left_parent->mbr.enlargement_needed(ptr->mbr) - la - a
                < right_parent->mbr.enlargement_needed(ptr->mbr) - ra - a) {
                left_block->nodes.push_back(ptr);
                left_block->count++;
                left_parent->mbr = Rectangle::calc_mbr(left_parent->mbr, ptr->mbr);
                ptr->parent = left_block;
            } else {
                right_block->nodes.push_back(ptr);
                right_block->count++;
                right_parent->mbr = Rectangle::calc_mbr(right_parent->mbr, ptr->mbr);
                ptr->parent = right_block;
            }
        }

        t->nodes.clear();
        t->count = 0;

        if (t->parent == nullptr) {
            delete t;
            root = new Block{.is_leaf = false,
                             .nodes = std::vector<Node*>{left_parent, right_parent},
                             .parent = nullptr,
                             .count = 2};
            left_parent->parent = root;
            right_parent->parent = root;
            return;
        }

        auto up_node = t->parent;
        auto up_block = up_node->parent;

        for (int i = 0; i < up_block->count; i++) {
            if (up_block->nodes[i] != up_node)
                continue;
            up_block->nodes[i] = left_parent;
            break;
        }

        up_block->nodes.push_back(right_parent);
        up_block->count++;

        left_parent->parent = up_block;
        right_parent->parent = up_block;

        up_block->is_leaf = false;

        up_node->children = nullptr;
        delete up_node;

        if (up_block->count > M)
            split(up_block);

        adjust_tree(up_block);

        // delete t;
    }
};

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
            f = f && equal(x[i], y[i]);
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

    Node(bool is_leaf, Rectangle mbr) : is_leaf(is_leaf), mbr(mbr) {}
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
    void insert(const Rectangle& mbr, T* elem);
    void remove(Rectangle r);

   private:
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
    Node* choose_leaf(Rectangle s, Node<T>* n = nullptr) {
        if (!n)
            n = root;
        if (!n)
            return nullptr;

        if (n->is_leaf)
            return n;

        double min_area = std::numeric_limits<double>::max();
        double min_enlarge = min_area;

        auto leaf = n;

        for (auto node : n->nodes) {
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
    void adjust_tree(Node<T>*);
    void condense_tree(Node<T>*);

    void split(Node<T>* t) {}

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
            for (auto node : nodes) {
                if (Rectangle::overlap(node->mbr, r)) {
                    auto leaf = find_leaf(r, node);
                    if (leaf)
                        return leaf;
                }
            }
        }

        return nullptr;
    }

    void reinsert_subtree(Node<T>* node);
};
}  // namespace Gutman