#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "rtree/rtree.h"
#include "rtree_hilbert/hilbert_curve.h"

using ll = long long;
using Point = std::vector<ll>;

namespace hilbert {

struct Rectangle {
    Point min;
    Point max;
    mutable ll hilbert_value;

    static bool overlap(const Rectangle& a, const Rectangle& b) {
        for (int i = 0; i < a.min.size(); i++) {
            if (a.max[i] < b.min[i] || a.min[i] > b.max[i])
                return false;
        }
        return true;
    }
};

template <typename T>
struct Node {
    ll lhv;
    Rectangle mbr;
    std::vector<Node*> children;
    std::vector<std::pair<Rectangle, T*>> elems;
    bool is_leaf;

    size_t count() { return is_leaf ? elems.size() : children.size(); }
};

template <typename T>
class HilbertRTree {
    Node<T>* root;
    HilbertCurve curve;
    int _max, _min;

   public:
    std::vector<T*> search(const Rectangle& search_rect) const {
        std::vector<T*> result;
        dispatch_search(search_rect, root, result);
        return result;
    }

    void insert(const Rectangle& rectangle, T* elem) {
        rectangle.hilbert_value = get_hilb_value(rectangle);
        auto leaf = choose_leaf(rectangle, root);

        if (leaf->count() == _max) {
        } else {
            // leaf->rects[leaf->num_entries] = r;
            leaf->elems.push_back({rectangle, elem});
            auto rect_hv = rectangle.hilbert_value;
            int i = leaf->count() - 2;
            while (i >= 0 && leaf->elems[i].first.hilbert_value > rect_hv) {
                leaf->rects[i + 1] = leaf->rects[i];
                i--;
            }
            leaf->rects[i + 1] = std::make_pair(rectangle, elem);

            if (rect_hv > leaf->lhv)
                leaf->lhv = rect_hv;
        }
    }

   private:
    void dispatch_search(const Rectangle& search_rect, Node<T>* p, std::vector<T*>& result) {
        if (p->is_leaf) {
            for (auto const& [rect, elem] : p->elems) {
                if (Rectangle::overlap(rect, search_rect)) {
                    result.push_back(elem);
                }
            }
        } else {
            for (auto node : p->children) {
                const auto rect = node->mbr;
                if (Rectangle::overlap(rect, search_rect)) {
                    // std::vector<T*> partial;
                    dispatch_search(search_rect, node, result);
                    // result.insert(partial.begin(), partial.end());
                }
            }
        }
    }

    Node<T>* choose_leaf(const Rectangle& rect, Node<T>* node) {
        if (node->is_leaf)
            return node;

        int count = node->count();
        int chosen_entry = count - 1;
        auto max_lhv = INT64_MAX;
        auto rect_hv = rect.hilbert_value;
        for (int i = 0; i < count; i++) {
            if (node->children[i]->lhv > rect_hv) {
                if (node->children[i]->lhv < max_lhv) {
                    chosen_entry = i;
                    max_lhv = node->children[i]->lhv;
                }
            }
        }
        return chosen_entry(rect, node->children[chosen_entry]);
    }

    ll get_hilb_value(const Rectangle& r) {
        Point p(r.min);

        for (int i = 0; i < p.size(); i++) {
            p[i] = (r.max[i] + r.min[i]) / 2;
        }

        return curve.index(p);
    }
};

}  // namespace hilbert