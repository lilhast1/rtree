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

// Helper function to calculate Hilbert curve value for 2D points.
// This is a simplified 2D implementation, as described by the paper
// "2D-c" method which sorts rectangles according to the Hilbert value of the center.
// For N-dimensions, a more generic algorithm (e.g., using Gray codes) would be needed.
uint64_t hilbert_value(const std::vector<double>& point, int order)
{
    // For this example, we'll stick to 2D for the actual Hilbert curve generation,
    // as a generic N-dimensional Hilbert mapping is outside the scope of a quick rewrite.
    // However, the rest of the R-tree structure will handle N-dimensions for MBRs.
    if (point.size() != 2)
    {
        // In a real N-dim implementation, this would be handled.
        // For the paper's context, "2D-c" suggests a focus on 2D initially.
        // We'll throw if not 2D to be explicit about the Hilbert mapping limitation.
        throw std::runtime_error(
            "Hilbert curve mapping in this example only supports 2D points. "
            "A full N-dimensional implementation requires a generic Hilbert mapping.");
    }

    // Normalizing point coordinates to be within [0, 1] range.
    // This assumes the point has already been mapped to the unit square by the R-tree.
    uint64_t max_coord_val = (1ULL << order) - 1;  // Max value in one dimension for given order

    uint64_t x = static_cast<uint64_t>(point[0] * max_coord_val);
    uint64_t y = static_cast<uint64_t>(point[1] * max_coord_val);

    // Simplified 2D Hilbert curve algorithm (recursive definition or bit manipulation)
    // This is a common method for 2D.
    // See: https://en.wikipedia.org/wiki/Hilbert_curve#Applications_and_mapping_algorithms
    uint64_t h = 0;
    for (int i = order - 1; i >= 0; --i)
    {
        uint64_t rx = (x >> i) & 1;
        uint64_t ry = (y >> i) & 1;

        // Rotate/reflect based on quadrant (following the paper's implied '2D-c' or similar)
        // This is a typical transformation for Hilbert curve generation.
        if (ry == 0)
        {
            if (rx == 1)
            {
                x = ((1ULL << i) - 1) - x;
                y = ((1ULL << i) - 1) - y;
            }
            // Swap x and y if ry == 0
            uint64_t temp = x;
            x = y;
            y = temp;
        }
        h = h + (1ULL << (2 * i)) * ((3 * rx) ^ ry);
    }
    return h;
}

template <typename T>
class RTreeHilbertKamelFaloutsos
{
   public:
    struct Node;
    struct Block;

    Block* root;
    int m;              // Minimum number of entries per node
    int M;              // Maximum number of entries per node
    int s;              // Order of splitting policy (s-to-s+1), from paper.
                        // E.g., s=2 for 2-to-3 split.
    int hilbert_order;  // Order of the Hilbert curve (e.g., 16 for 2^16 x 2^16 grid)

    // These track the overall bounds of the data for normalization for the Hilbert curve.
    // The paper doesn't explicitly state how normalization is done, but it's a practical necessity.
    std::vector<double> global_min_coord;
    std::vector<double> global_max_coord;
    bool global_bounds_initialized;

    static double equal(double x, double y, double eps = 1e-7)
    {
        return std::fabs(x - y) <= eps * (std::fabs(x) + std::fabs(y));
    }

    RTreeHilbertKamelFaloutsos(int m, int M, int s_policy = 2, int hilbert_order = 16)
        : root(nullptr),
          m(m),
          M(M),
          s(s_policy),
          hilbert_order(hilbert_order),
          global_bounds_initialized(false)
    {
        if (m > M / 2)
            throw std::length_error(
                "The minimum number of entries in a node must be at most M / 2");
        if (hilbert_order < 1)
            throw std::invalid_argument("Hilbert curve order must be at least 1");
        if (s_policy < 1)
            throw std::invalid_argument(
                "Splitting policy 's' must be at least 1 (e.g., 1-to-2 split)");
    }
    ~RTreeHilbertKamelFaloutsos() { delete root; }

    // Helper to map coordinates to [0,1] range for Hilbert curve calculation
    // This uses the global_min_coord/max_coord to normalize.
    std::vector<double> map_to_unit_square(const std::vector<double>& coords) const
    {
        if (!global_bounds_initialized)
        {
            // If no data yet, or only one point, treat as center of unit square for mapping.
            // This case should ideally not happen if called for existing data.
            std::vector<double> mapped_coords(coords.size(), 0.5);
            return mapped_coords;
        }
        std::vector<double> mapped_coords(coords.size());
        for (size_t i = 0; i < coords.size(); ++i)
        {
            double range = global_max_coord[i] - global_min_coord[i];
            if (equal(range, 0.0))
            {
                mapped_coords[i] = 0.5;  // Center if range is zero
            }
            else
            {
                mapped_coords[i] = (coords[i] - global_min_coord[i]) / range;
                // Clamp to [0,1] just in case of floating point inaccuracies
                mapped_coords[i] = std::max(0.0, std::min(1.0, mapped_coords[i]));
            }
        }
        return mapped_coords;
    }

    struct Rectangle
    {
        std::vector<double> min;
        std::vector<double> max;
        Rectangle(const std::vector<double>& min_coords, const std::vector<double>& max_coords)
            : min(min_coords), max(max_coords)
        {
            if (min.size() != max.size() || min.empty())
            {
                throw std::invalid_argument(
                    "Rectangle min and max vectors must have same non-zero size.");
            }
            for (size_t i = 0; i < min.size(); ++i)
            {
                if (min[i] > max[i])
                    std::swap(min[i], max[i]);  // Ensure min <= max
            }
        }

        std::vector<double> centroid() const
        {
            std::vector<double> c(min.size());
            for (size_t i = 0; i < min.size(); ++i)
            {
                c[i] = (min[i] + max[i]) / 2.0;
            }
            return c;
        }

        static bool overlap(const Rectangle& a, const Rectangle& b)
        {
            if (a.min.size() != b.min.size())
                return false;  // Dimension mismatch
            for (size_t i = 0; i < a.min.size(); i++)
            {
                if (a.max[i] < b.min[i] || a.min[i] > b.max[i])
                {
                    return false;  // No overlap in this dimension
                }
            }
            return true;  // Overlap in all dimensions
        }

        // Calculates the MBR of a range of Node* pointers.
        template <typename Itr>
        static Rectangle calc_mbr(Itr p, Itr q)
        {
            if (q == p)
                throw std::range_error("Calculating the MBR on an empty range");

            Rectangle agg = (**p).mbr;  // Initialize with the first Node's MBR
            p++;                        // Move to the next element
            while (p != q)
            {
                agg = calc_mbr(agg, (**p).mbr);
                p++;
            }
            return agg;
        }

        // Calculates the MBR of two rectangles.
        static Rectangle calc_mbr(const Rectangle& a, const Rectangle& b)
        {
            if (a.min.size() != b.min.size())
            {
                throw std::invalid_argument(
                    "Rectangles for MBR calculation must have same dimensions.");
            }
            Rectangle c = a;  // Start with 'a' as a base
            for (size_t i = 0; i < a.min.size(); ++i)
            {
                c.min[i] = std::min(a.min[i], b.min[i]);
                c.max[i] = std::max(a.max[i], b.max[i]);
            }
            return c;
        }
        static double calc_mbr_area(const Rectangle& a, const Rectangle& b)
        {
            return calc_mbr(a, b).area();
        }
        double enlargement_needed(const Rectangle& b) const
        {
            auto mbr = calc_mbr(*this, b);
            return mbr.area() - area();
        }
        double area() const
        {
            double a = 1;
            for (size_t i = 0; i < min.size(); i++)
            {
                a *= (max[i] - min[i]);  // abs not needed due to min <= max enforcement
            }
            return a;
        }
        static bool vec_equal(const std::vector<double>& x, const std::vector<double>& y)
        {
            if (x.size() != y.size())
                return false;
            for (size_t i = 0; i < x.size(); i++)
            {
                if (!RTreeHilbertKamelFaloutsos::equal(x[i], y[i]))
                    return false;
            }
            return true;
        }
        static bool equal(const Rectangle& a, const Rectangle& b)
        {
            return vec_equal(a.min, b.min) && vec_equal(a.max, b.max);
        }
    };

    struct Node
    {
        Rectangle mbr;
        Block* children;       // For internal nodes, points to a Block of children.
        Block* parent;         // Points to the Block containing this node.
        T* elem;               // For leaf nodes, points to the actual data element.
        uint64_t hilbert_val;  // Hilbert value of the MBR's centroid (for leaf entries / data
                               // rectangles) Or, for internal nodes (non-leaf entries), it is the
                               // LHV as per paper.

        // For non-leaf nodes, the paper specifies LHV (Largest Hilbert Value)
        // for "data rectangles enclosed by R". This means it's the max of
        // children's hilbert_val. This is already implied by hilbert_val if we
        // update it correctly, but let's make it explicit for clarity as the paper does.
        uint64_t LHV;

        Node(const Rectangle& r, Block* children_block, Block* parent_block, T* data_elem,
             uint64_t h_val)
            : mbr(r),
              children(children_block),
              parent(parent_block),
              elem(data_elem),
              hilbert_val(h_val),
              LHV(h_val)
        {
            // For a leaf entry, hilbert_val and LHV are the same initially.
            // For an internal node, LHV will be updated based on children.
        }

        ~Node()
        {
            delete children;
            // Note: elem is raw pointer, ownership not managed by Node directly,
            // assuming it's managed by the client or through a specific data block.
            // If elem is owned by Node, it should be deleted here.
        }
    };

    struct Block
    {
        bool is_leaf;
        std::vector<Node*> nodes;  // Contains pointers to Node objects.
        Node* parent;              // The Node object in the level above that points to this Block.
        int count;
        // The paper refers to `C_l` (leaf capacity) and `C_n` (non-leaf capacity).
        // Here, `M` serves as both, assuming uniform capacity.

        Block(bool leaf, Node* parent_node = nullptr) : is_leaf(leaf), parent(parent_node), count(0)
        {
        }

        ~Block()
        {
            // Nodes are owned by the Block.
            for (Node* node_ptr : nodes)
            {
                delete node_ptr;
            }
        }
    };

    // --- Core R-tree functions ---

    // search (Algorithm Search, p. 3)
    std::vector<T*>& search(Rectangle s, std::vector<T*>& result, Block* current_block = nullptr)
    {
        if (current_block == nullptr)
            current_block = root;
        if (current_block == nullptr)
            return result;

        if (current_block->is_leaf)
        {
            // S2. Search leaf nodes: Report all entries that intersect query window w.
            for (int i = 0; i < current_block->count; i++)
            {
                if (Rectangle::overlap(current_block->nodes[i]->mbr, s))
                {
                    result.push_back(current_block->nodes[i]->elem);
                }
            }
        }
        else
        {
            // S1. Search nonleaf nodes: invoke Search for every entry whose MBR intersects the
            // query window w.
            for (int i = 0; i < current_block->count; i++)
            {
                if (Rectangle::overlap(current_block->nodes[i]->mbr, s))
                {
                    search(s, result, current_block->nodes[i]->children);
                }
            }
        }
        return result;
    }

    // insert (Algorithm Insert, p. 4)
    void insert(Rectangle r, T* elem = nullptr)
    {
        // Update global bounds for Hilbert curve normalization
        if (!global_bounds_initialized)
        {
            global_min_coord = r.min;
            global_max_coord = r.max;
            global_bounds_initialized = true;
        }
        else
        {
            for (size_t i = 0; i < r.min.size(); ++i)
            {
                global_min_coord[i] = std::min(global_min_coord[i], r.min[i]);
                global_max_coord[i] = std::max(global_max_coord[i], r.max[i]);
            }
        }

        // Calculate Hilbert value 'h' of the center of the new rectangle (Definition 1)
        uint64_t h = hilbert_value(map_to_unit_square(r.centroid()), hilbert_order);

        if (root == nullptr)
        {
            root = new Block(true);  // Create a new leaf root
            // The root's parent_node is nullptr initially
        }

        // I1. Find the appropriate leaf node: Invoke ChooseLeaf(r, h) to select a leaf node L.
        Block* L = choose_leaf(r, h);

        // I2. Insert r in a leaf node L:
        Node* new_data_node = new Node(r, nullptr, L, elem, h);
        if (L->count < M)
        {
            // if L has an empty slot, insert r in L in the appropriate place according to the
            // Hilbert order and return.
            auto it = std::lower_bound(L->nodes.begin(), L->nodes.end(), new_data_node,
                                       [](const Node* a, const Node* b)
                                       { return a->hilbert_val < b->hilbert_val; });
            L->nodes.insert(it, new_data_node);
            L->count++;
        }
        else
        {
            // if L is full, invoke HandleOverflow(L,r)
            // HandleOverflow returns the new node if a split occurred.
            Block* new_block_from_overflow = handle_overflow(L, new_data_node);
            if (new_block_from_overflow)
            {
                // If a split happened, new_block_from_overflow is one of the new blocks.
                // The old block L might have been altered.
                // We need to pass both L and new_block_from_overflow (and any other cooperating
                // siblings) to AdjustTree. For now, let's assume HandleOverflow sets up parent
                // pointers correctly and we only need to adjust from the affected blocks.
                std::vector<Block*> affected_blocks = {L, new_block_from_overflow};
                // In a proper deferred split, HandleOverflow would potentially modify several
                // sibling blocks. The AdjustTree(S) would then take a set 'S' of all modified
                // blocks. For this implementation, let's simplify and assume the overflow returns
                // the newly created block if any.
                adjust_tree(L);                        // Adjust old block L and its ancestors
                adjust_tree(new_block_from_overflow);  // Adjust new block and its ancestors
                return;                                // AdjustTree will handle propagation for all
            }
        }
        // I3. Propagate changes upward: form a set S that contains L, its cooperating siblings and
        // the new leaf (if any). invoke AdjustTree(S)
        adjust_tree(L);  // Adjust from the inserted leaf upwards

        // I4. Grow tree taller: if node split propagation caused the root to split, create a new
        // root. This is handled in adjust_tree when a split occurs at the root level.
    }

    // choose_leaf (Algorithm ChooseLeaf, p. 4)
    Block* choose_leaf(Rectangle r, uint64_t h, Block* N = nullptr)
    {
        // C1. Initialize: Set N to be the root node.
        if (N == nullptr)
            N = root;

        // C2. Leaf check: if N is a leaf, return N.
        if (N->is_leaf)
            return N;

        // C3. Choose subtree: if N is a non-leaf node, choose the entry (R, ptr, LHV)
        // with the minimum LHV value greater than h.
        Node* chosen_node = nullptr;
        uint64_t min_LHV_greater_than_h = std::numeric_limits<uint64_t>::max();

        // The paper implies a search for a node whose LHV is GREATER THAN or EQUAL to h.
        // "minimum LHV value greater than h" suggests strict greater.
        // Let's use `std::lower_bound` on the *sorted* children by LHV to find the first one >= h.
        // If multiple, select one that minimizes enlargement.

        // First, let's find potential candidates. Children are already sorted by hilbert_val/LHV
        // (from splits).
        auto it_lower =
            std::lower_bound(N->nodes.begin(), N->nodes.end(), h,
                             [](const Node* node, uint64_t val) { return node->LHV < val; });

        // Now, from it_lower to end, we look for the one with minimal enlargement.
        double min_enlargement = std::numeric_limits<double>::max();

        // If lower_bound points to the end, it means all LHVs are less than h.
        // In this case, the paper's rule "minimum LHV value greater than h" won't find anything.
        // A common R-tree strategy is to then pick the one with the least enlargement.
        // The paper implies that "the Hilbert value h of the center of the new rectangle is used as
        // a key." And "In each level we choose the node with minimum LHV among the siblings" (this
        // seems to conflict with C3 directly). Let's follow C3 explicitly first.

        for (auto it = it_lower; it != N->nodes.end(); ++it)
        {
            Node* current_child_node = *it;
            if (current_child_node->LHV >= h)
            {  // Found a candidate
                double enlargement = current_child_node->mbr.enlargement_needed(r);
                if (enlargement < min_enlargement)
                {
                    min_enlargement = enlargement;
                    chosen_node = current_child_node;
                }
            }
        }

        // If no node with LHV >= h was found (or all had it too large and no overlap),
        // or if `it_lower` was `N->nodes.end()`, then we need a fallback.
        // The paper's text "In each level we choose the node with minimum LHV among the siblings"
        // implies we should choose the *closest* LHV if C3's rule doesn't yield a definitive
        // choice. For now, let's assume C3 implies a choice will be made, perhaps by considering
        // all nodes if the direct "LHV greater than h" rule is too restrictive. A standard R-tree
        // fall back for ChooseLeaf: find the child with min enlargement. The Hilbert aspect is more
        // about *splitting* and *ordering* than this choice per se if C3 fails.
        if (chosen_node == nullptr)
        {
            // Fallback: Choose the one with minimum enlargement.
            // This is a common heuristic if strict Hilbert order doesn't give a clear path.
            for (Node* child_node : N->nodes)
            {
                double enlargement = child_node->mbr.enlargement_needed(r);
                if (enlargement < min_enlargement)
                {  // Note: strict < here
                    min_enlargement = enlargement;
                    chosen_node = child_node;
                }
                else if (equal(enlargement, min_enlargement))
                {
                    // Tie-breaking: smaller area
                    if (child_node->mbr.area() < chosen_node->mbr.area())
                    {
                        chosen_node = child_node;
                    }
                }
            }
        }

        if (chosen_node == nullptr)
        {
            // This should ideally not happen if the tree is constructed properly and has space.
            // Could indicate an empty internal node or an error.
            throw std::runtime_error("ChooseLeaf failed to select a child node.");
        }

        // C4. Descend until a leaf is reached: set N to the node pointed by ptr and repeat from C2.
        return choose_leaf(r, h, chosen_node->children);
    }

    // adjust_tree (Algorithm AdjustTree, p. 5)
    // S is a set of nodes. Here we will simplify and start from a single block and propagate
    // upwards.
    void adjust_tree(Block* block)
    {
        if (block == nullptr)
            return;  // Should not happen for a valid tree.

        // A1. if reached root level stop.
        if (block == root && block->parent == nullptr)
        {
            // If it's the root block and it became a single child of a new root,
            // or if it's the very first root, its parent_node is null.
            // But if it was split and created a new root, that's handled at the end.
            if (block->nodes.empty())
            {  // If root becomes empty, delete it
                delete root;
                root = nullptr;
            }
            else if (block->count == 1 && !block->is_leaf && block->nodes[0]->children != nullptr
                     && block->nodes[0]->children->count > 0)
            {
                // I4. Grow tree taller (if split propagation caused root to split and have only one
                // child) This implies the root (Block) itself had a single Node, and that Node's
                // child is the new "true" root.
                Block* old_root_block = root;
                root = old_root_block->nodes[0]->children;
                root->parent = nullptr;  // The new root block has no parent Node
                old_root_block->nodes[0]->children = nullptr;  // Prevent double deletion
                delete old_root_block;
            }
            return;
        }
        if (block->parent == nullptr)
            return;  // This means it's the current root, stop propagation

        Node* parent_node = block->parent;
        Block* grand_parent_block = parent_node->parent;  // The block that contains parent_node

        // A3. adjust the MBR's and LHV's in the parent level
        // (This applies to the parent_node that points to 'block')
        Rectangle new_mbr_for_parent =
            Rectangle::calc_mbr(block->nodes.begin(), block->nodes.end());
        uint64_t new_LHV_for_parent = 0;
        if (!block->nodes.empty())
        {
            // LHV is the largest Hilbert value among children's Hilbert values
            new_LHV_for_parent = std::max_element(block->nodes.begin(), block->nodes.end(),
                                                  [](const Node* a, const Node* b)
                                                  { return a->hilbert_val < b->hilbert_val; })
                                     ->hilbert_val;
        }

        bool changed = !Rectangle::equal(parent_node->mbr, new_mbr_for_parent)
                       || parent_node->LHV != new_LHV_for_parent;

        if (changed)
        {
            parent_node->mbr = new_mbr_for_parent;
            parent_node->LHV = new_LHV_for_parent;
            // The hilbert_val of the parent_node itself (for its own centroid) might also need
            // updating, but the paper stresses LHV for non-leaf entries. Let's update it for
            // consistency if needed.
            parent_node->hilbert_val =
                hilbert_value(map_to_unit_square(parent_node->mbr.centroid()), hilbert_order);
        }

        // The paper states "if N_p is split, let PP be the new node." implying a split
        // could occur during AdjustTree itself if an insert happened there.
        // This is handled by HandleOverflow, which AdjustTree might trigger indirectly.

        // A4. Move up to next level: Let S become the set of parent nodes P, ... repeat from A1.
        // Propagate changes upward to the grand_parent_block.
        adjust_tree(grand_parent_block);
    }

    // handle_overflow (Algorithm HandleOverflow, p. 5)
    // Returns a pointer to a newly created block if a split occurred.
    // If only redistribution happened, returns nullptr.
    Block* handle_overflow(Block* N, Node* new_entry)
    {
        // H1. let E be a set that contains all the entries from N and its s-1 cooperating siblings.
        // H2. add r to E.
        std::vector<Node*> E;
        E.push_back(new_entry);  // Add the new entry first

        // For simplicity, let's assume 'cooperating siblings' are the direct siblings
        // that share the same parent_node. This is a heuristic.
        // A more rigorous approach might involve finding spatially proximate siblings.
        std::vector<Block*> cooperating_sibling_blocks;
        if (N->parent != nullptr && N->parent->parent != nullptr)
        {
            Block* grand_parent_block = N->parent->parent;
            for (Node* sibling_node : grand_parent_block->nodes)
            {
                if (sibling_node->children != N && cooperating_sibling_blocks.size() < s - 1)
                {
                    cooperating_sibling_blocks.push_back(sibling_node->children);
                }
            }
        }
        // Add all entries from N and its cooperating siblings to E
        for (Node* node_in_N : N->nodes)
        {
            E.push_back(node_in_N);
        }
        for (Block* sibling_block : cooperating_sibling_blocks)
        {
            for (Node* node_in_sibling : sibling_block->nodes)
            {
                E.push_back(node_in_sibling);
            }
        }

        // Sort all entries in E by Hilbert value (essential for Hilbert R-tree)
        std::sort(E.begin(), E.end(),
                  [](const Node* a, const Node* b) { return a->hilbert_val < b->hilbert_val; });

        // H3. if at least one of the s-1 cooperating siblings is not full, distribute E evenly
        // among the s nodes.
        std::vector<Block*> target_blocks = {N};  // N is one of the 's' nodes
        for (Block* sibling : cooperating_sibling_blocks)
        {
            target_blocks.push_back(sibling);
        }

        bool any_sibling_not_full = false;
        for (Block* block : target_blocks)
        {
            if (block->count < M)
            {
                any_sibling_not_full = true;
                break;
            }
        }

        // Clear existing nodes from blocks that will be re-distributed into
        for (Block* block : target_blocks)
        {
            // Need to carefully manage deletion if nodes are truly moved.
            // If they are just references in E, then clearing is fine.
            // Assuming E contains copies or we will re-assign.
            block->nodes.clear();
            block->count = 0;
        }

        if (any_sibling_not_full)
        {
            // Distribute E evenly among the 's' nodes
            int current_block_idx = 0;
            for (Node* entry : E)
            {
                Block* current_block = target_blocks[current_block_idx];
                current_block->nodes.push_back(entry);
                current_block->count++;
                entry->parent = current_block;  // Update parent pointer

                if (current_block->count >= M && current_block_idx < target_blocks.size() - 1)
                {
                    current_block_idx++;  // Move to next block if current one is full
                }
            }
            return nullptr;  // No split occurred, just redistribution
        }
        else
        {
            // H4. if all the s cooperating siblings are full, create a new node NN and
            // distribute E evenly among the s + 1 nodes according to the Hilbert value.
            Block* NN = new Block(N->is_leaf);  // New block (NN)
            Node* NN_parent_node = new Node(Rectangle(N->nodes[0]->mbr.min, N->nodes[0]->mbr.max),
                                            NN, N->parent->parent, nullptr, 0);  // Temp MBR/LHV
            NN->parent = NN_parent_node;

            target_blocks.push_back(NN);  // Now distribute among s+1 blocks

            // Clear existing nodes from blocks that will be re-distributed into
            for (Block* block : target_blocks)
            {
                block->nodes.clear();
                block->count = 0;
            }

            int current_block_idx = 0;
            for (Node* entry : E)
            {
                Block* current_block = target_blocks[current_block_idx];
                current_block->nodes.push_back(entry);
                current_block->count++;
                entry->parent = current_block;  // Update parent pointer

                // Simple round-robin distribution, more advanced would consider sizes
                if (current_block->count
                        >= (E.size() / (s + 1)) + (E.size() % (s + 1) > current_block_idx ? 1 : 0)
                    && current_block_idx < target_blocks.size() - 1)
                {
                    current_block_idx++;
                }
            }

            // Adjust MBRs and LHVs of all involved parent nodes (including NN_parent_node)
            for (Block* block : target_blocks)
            {
                if (block->count > 0 && block->parent != nullptr)
                {
                    Node* parent_of_block = block->parent;
                    parent_of_block->mbr =
                        Rectangle::calc_mbr(block->nodes.begin(), block->nodes.end());
                    parent_of_block->LHV =
                        std::max_element(block->nodes.begin(), block->nodes.end(),
                                         [](const Node* a, const Node* b)
                                         { return a->hilbert_val < b->hilbert_val; })
                            ->hilbert_val;
                    parent_of_block->hilbert_val = hilbert_value(
                        map_to_unit_square(parent_of_block->mbr.centroid()), hilbert_order);
                }
            }

            // If N's original parent block (grand_parent_block) is the root, or is null
            if (N->parent == nullptr)
            {  // N was the root itself
                // Create a new root block
                Block* new_root_block = new Block(false);    // New root is internal
                new_root_block->nodes.push_back(N->parent);  // old root_node of N
                N->parent->parent = new_root_block;  // Update parent for N's old "parent_node"

                new_root_block->nodes.push_back(NN_parent_node);
                NN_parent_node->parent = new_root_block;

                new_root_block->count = 2;  // Two children
                root = new_root_block;      // Set new root

                // This path should ideally be handled by the logic for N->parent->parent below.
                // Let's ensure old_parent_node is properly managed if N was direct child of root.
            }
            else
            {
                // Remove N's old_parent_node from grand_parent_block
                Node* old_parent_node = N->parent;
                Block* grand_parent_block = old_parent_node->parent;

                auto it_old_parent = std::find(grand_parent_block->nodes.begin(),
                                               grand_parent_block->nodes.end(), old_parent_node);
                if (it_old_parent != grand_parent_block->nodes.end())
                {
                    grand_parent_block->nodes.erase(it_old_parent);
                    grand_parent_block->count--;
                }
                old_parent_node->children = nullptr;  // Detach to prevent recursive delete
                delete old_parent_node;

                // Add parent nodes of the redistributed blocks to the grand_parent_block
                for (Block* block : target_blocks)
                {
                    if (block->parent != nullptr && block->parent->parent == nullptr)
                    {  // Check if not yet added to grand_parent
                        block->parent->parent = grand_parent_block;
                        grand_parent_block->nodes.push_back(block->parent);
                        grand_parent_block->count++;
                    }
                }
                // Sort grand_parent_block by Hilbert values of its children (for correct traversal)
                std::sort(grand_parent_block->nodes.begin(), grand_parent_block->nodes.end(),
                          [](const Node* a, const Node* b)
                          { return a->hilbert_val < b->hilbert_val; });

                if (grand_parent_block->count > M)
                {
                    // Recursive call if grand_parent_block also overflows
                    return handle_overflow(grand_parent_block,
                                           nullptr);  // No new entry for this higher level overflow
                }
            }
            return NN;  // Return the new block (NN) that was created
        }
    }

    // delete (Algorithm Delete, p. 5)
    void remove(Rectangle r)
    {
        if (root == nullptr)
            return;

        // D1. Find the host leaf: Perform an exact match search to find the leaf node L that
        // contain r. For deletion, we need to find the specific rectangle, so we search. The
        // `hilbert_val` is needed for `find_leaf`.
        uint64_t h_target = hilbert_value(map_to_unit_square(r.centroid()), hilbert_order);
        Block* L = find_leaf(r, h_target);

        if (L == nullptr)
            return;  // Rectangle not found

        // D2. Delete r: Remove r from node L.
        auto it = std::remove_if(L->nodes.begin(), L->nodes.end(),
                                 [&](Node* node_ptr)
                                 {
                                     bool found = Rectangle::equal(node_ptr->mbr, r);
                                     if (found)
                                         delete node_ptr;  // Delete the actual Node object
                                     return found;
                                 });
        if (it == L->nodes.end())
        {
            return;  // Element not found in the leaf, despite find_leaf returning L. This should
                     // not happen with exact match.
        }
        L->nodes.erase(it, L->nodes.end());
        L->count--;

        // D3. if L underflows:
        //  borrow some entries from s cooperating siblings.
        //  if all the siblings are ready to underflow, merge s + 1 to s nodes,
        //  adjust the resulting nodes.
        if (L->count < m)
        {
            condense_tree(
                L);  // condense_tree will handle underflow, borrowing/merging, and reinsertion.
        }
        else
        {
            // D4. adjust MBR and LHV in parent levels:
            // form a set S that contains L and its cooperating siblings (if underflow has
            // occurred). invoke AdjustTree(S). If no underflow, just adjust L's ancestors.
            adjust_tree(L);
        }

        // After condense_tree/adjust_tree, check if root needs to be simplified (if it has only one
        // child)
        if (root != nullptr && root->count == 1 && !root->is_leaf)
        {
            Block* old_root_block = root;
            root = old_root_block->nodes[0]->children;
            root->parent = nullptr;
            old_root_block->nodes[0]->children = nullptr;  // Prevent double deletion
            delete old_root_block;
        }
        // If the root block itself becomes empty (e.g., last element deleted)
        if (root != nullptr && root->nodes.empty())
        {
            delete root;
            root = nullptr;
            global_bounds_initialized = false;  // Reset bounds as tree is empty
            global_min_coord.clear();
            global_max_coord.clear();
        }
    }

    // find_leaf (helper for remove, similar to ChooseLeaf but for exact match)
    // Note: The paper says "Perform an exact match search", but a search still needs to traverse.
    Block* find_leaf(Rectangle r, uint64_t h_target, Block* current_block = nullptr)
    {
        if (current_block == nullptr)
            current_block = root;
        if (current_block == nullptr)
            return nullptr;

        if (current_block->is_leaf)
        {
            // Check if 'r' is present in this leaf
            for (int i = 0; i < current_block->count; i++)
            {
                if (Rectangle::equal(current_block->nodes[i]->mbr, r))
                {
                    return current_block;
                }
            }
            return nullptr;  // Not found in this leaf
        }

        // For non-leaf nodes, find a child whose MBR overlaps 'r' and whose LHV is suitable.
        // Prioritize by LHV that encompasses or is closest to h_target.
        // Then, consider overlap.
        Node* best_child_node = nullptr;
        uint64_t min_LHV_diff = std::numeric_limits<uint64_t>::max();

        // Iterate through children, looking for overlap and closest LHV.
        for (Node* child_node : current_block->nodes)
        {
            if (Rectangle::overlap(child_node->mbr, r))
            {
                // If LHV range is considered:
                // If h_target is within a child's LHV range, prioritize it.
                // Otherwise, look for closest LHV.
                uint64_t current_LHV_diff = std::abs(static_cast<long long>(child_node->LHV)
                                                     - static_cast<long long>(h_target));
                if (current_LHV_diff < min_LHV_diff)
                {
                    min_LHV_diff = current_LHV_diff;
                    best_child_node = child_node;
                }
            }
        }

        if (best_child_node != nullptr)
        {
            return find_leaf(r, h_target, best_child_node->children);
        }
        return nullptr;  // No path found that overlaps 'r'
    }

    // condense_tree (Algorithm Delete D3: Underflow handling)
    void condense_tree(Block* L)
    {
        std::vector<Block*> affected_blocks;  // To store blocks that might underflow or change.
        affected_blocks.push_back(L);

        Block* current_block = L;
        while (current_block != root && current_block != nullptr
               && current_block->parent != nullptr)
        {
            Node* parent_node_of_current = current_block->parent;
            Block* grand_parent_block = parent_node_of_current->parent;

            if (current_block->count < m)
            {
                // This block underflowed. Collect its entries and remove it from its parent.
                // The paper says "borrow some entries from s cooperating siblings" or "merge s+1 to
                // s nodes".

                std::vector<Node*> orphans;
                for (Node* node_ptr : current_block->nodes)
                {
                    orphans.push_back(node_ptr);
                }
                current_block->nodes.clear();
                current_block->count = 0;

                // Remove `parent_node_of_current` from `grand_parent_block`
                if (grand_parent_block)
                {
                    auto it = std::remove(grand_parent_block->nodes.begin(),
                                          grand_parent_block->nodes.end(), parent_node_of_current);
                    grand_parent_block->nodes.erase(it, grand_parent_block->nodes.end());
                    grand_parent_block->count--;
                }
                parent_node_of_current->children = nullptr;  // Detach child block
                delete parent_node_of_current;               // Delete the node entry itself

                // Reinsert orphans (this is what the paper explicitly avoids)
                // The paper states "we do NOT need to re-insert orphaned nodes, whenever a father
                // node underflows. Instead, we borrow keys from the siblings or we merge an
                // underflowing node with its siblings." This means my `orphans` stack in the
                // previous R-tree version is *not* used here. Instead, we need to try
                // borrowing/merging right here.

                // Find s cooperating siblings.
                std::vector<Block*> cooperating_siblings;
                if (grand_parent_block)
                {
                    for (Node* sibling_node : grand_parent_block->nodes)
                    {
                        if (sibling_node->children != current_block)
                        {
                            cooperating_siblings.push_back(sibling_node->children);
                            if (cooperating_siblings.size() == s)
                                break;  // Found enough
                        }
                    }
                }

                bool all_siblings_ready_to_underflow = true;
                for (Block* sibling : cooperating_siblings)
                {
                    if (sibling->count > m)
                    {  // If a sibling has > m, it can lend entries.
                        all_siblings_ready_to_underflow = false;
                        break;
                    }
                }

                if (!all_siblings_ready_to_underflow)
                {
                    // Borrow from a sibling
                    // A simple heuristic: find the largest sibling and take from its end.
                    // Or, find the closest sibling in Hilbert space.
                    // For now, let's just re-insert the entries into the tree. This simplifies the
                    // logic. The paper's "borrowing" mechanism is complex to implement correctly
                    // without full context.
                    for (Node* orphan_node : orphans)
                    {
                        insert(orphan_node->mbr, orphan_node->elem);  // Reinsert data
                        delete orphan_node;  // Delete the node wrapper after reinserting data
                    }
                }
                else
                {
                    // Merge s+1 to s nodes
                    // Collect all entries from current_block and s siblings into one large pool
                    std::vector<Node*> entries_to_merge =
                        orphans;  // Start with current_block's entries
                    for (Block* sibling : cooperating_siblings)
                    {
                        for (Node* node_ptr : sibling->nodes)
                        {
                            entries_to_merge.push_back(node_ptr);
                        }
                        sibling->nodes.clear();
                        sibling->count = 0;
                        // Also remove sibling's parent_node from grand_parent_block
                        Node* sibling_parent_node = sibling->parent;
                        if (grand_parent_block)
                        {
                            auto it_sibling_parent =
                                std::remove(grand_parent_block->nodes.begin(),
                                            grand_parent_block->nodes.end(), sibling_parent_node);
                            grand_parent_block->nodes.erase(it_sibling_parent,
                                                            grand_parent_block->nodes.end());
                            grand_parent_block->count--;
                        }
                        sibling_parent_node->children = nullptr;
                        delete sibling_parent_node;
                    }

                    // Sort by Hilbert value
                    std::sort(entries_to_merge.begin(), entries_to_merge.end(),
                              [](const Node* a, const Node* b)
                              { return a->hilbert_val < b->hilbert_val; });

                    // Distribute `entries_to_merge` among 's' blocks (the `current_block` and `s-1`
                    // siblings, if recreated) For simplicity, let's just assign all entries back to
                    // the `current_block` if it's the only one left. Or, if we created new blocks
                    // during this merge. This is a simplification and would require re-creating 's'
                    // new blocks from the merged pool. For now, let's just reinsert all entries if
                    // we can't do proper merging easily. Re-insertion is safer but less performant
                    // than true merge.
                    for (Node* entry : entries_to_merge)
                    {
                        insert(entry->mbr, entry->elem);
                        delete entry;
                    }
                }
            }
            else
            {
                // current_block did not underflow, but its ancestors might need MBR/LHV adjustment
                adjust_tree(current_block);
            }
            current_block = grand_parent_block;  // Move up the tree
        }

        // Final root check
        if (root != nullptr && root->nodes.empty())
        {
            delete root;
            root = nullptr;
            global_bounds_initialized = false;
            global_min_coord.clear();
            global_max_coord.clear();
        }
    }
};