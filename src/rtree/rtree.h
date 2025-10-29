#include <any>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

template <typename T>
class RTreeIteratorBase {
   public:
    virtual ~RTreeIteratorBase() = default;
    virtual bool operator==(const RTreeIteratorBase& other) const = 0;
    virtual RTreeIteratorBase& operator++() = 0;
    virtual RTreeIteratorBase& operator++(int) = 0;
    virtual const T& operator*() const = 0;
    virtual std::unique_ptr<RTreeIteratorBase> clone() const = 0;
};

template <typename T>
class RTree {
   protected:
    virtual const RTreeIteratorBase<T> queryImpl(
        const std::function<bool(const T&)>& pred) const = 0;

   public:
    RTree() = default;

    template <typename Itr>
    RTree(Itr first, Itr last) {
        insert(first, last);
    }

    RTree(std::initializer_list<T> list) { insert(list.begin(), list.end()); }
    RTree(const RTree<T>&) = default;
    RTree(RTree<T>&&) = default;
    RTree<T>& operator=(const RTree<T>&) = default;
    RTree<T>& operator=(RTree<T>&&) = default;
    virtual ~RTree() = default;

    virtual void swap(RTree<T>&) = 0;

    virtual void insert(const T&) = 0;

    template <typename Itr>
    void insert(Itr first, Itr last) {
        while (first != last) {
            this->insert(*first++);
        }
    }

    virtual void remove(const T&) = 0;

    virtual RTreeIteratorBase<T> begin() = 0;
    virtual RTreeIteratorBase<T> end() = 0;

    virtual const RTreeIteratorBase<T> begin() const = 0;
    virtual const RTreeIteratorBase<T> end() const = 0;

    template <typename Predicate>
    RTreeIteratorBase<T> query(Predicate pred) {
        return queryImpl(pred);
    }

    template <typename Predicate>
    const RTreeIteratorBase<T> query(Predicate pred) const {
        return queryImpl(pred);
    }

    virtual unsigned int size() const = 0;

    virtual bool empty() const = 0;
    virtual void clear() = 0;
};

template <typename T>
struct MBR {
    std::vector<double> coord;
    std::string id;
    void* children;
    T* data;
};

template <typename T>
class VanillaNode {
    std::vector<MBR<T>> mbrs;
};

template <typename T>
class RTreeVanilla : public RTree<T> {
    const RTreeIteratorBase<T> queryImpl(const std::function<bool(const T&)>& pred);

    static std::vector<MBR<T>> search(const VanillaNode<T> root,
                                      const std::vector<double>& rectangle);

    unsigned int _size;
    unsigned int m;
    unsigned int M;

   public:
    RTreeVanilla();
    RTreeVanilla(const RTree<T>&);
    RTreeVanilla(RTree<T>&&);
    RTreeVanilla<T>& operator=(const RTree<T>&);
    RTreeVanilla<T>& operator=(RTree<T>&&);
    ~RTreeVanilla();

    void swap(RTreeVanilla<T>&) override;

    void insert(const T&) override;

    void remove(const T&) override;

    RTreeIteratorBase<T> begin() override;
    RTreeIteratorBase<T> end() override;

    const RTreeIteratorBase<T> begin() const override;
    const RTreeIteratorBase<T> end() const override;

    unsigned int size() const override;

    bool empty() const override;
    void clear() override;
};