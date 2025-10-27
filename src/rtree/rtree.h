#include <any>
#include <initializer_list>
#include <iterator>
#include <vector>

template <typename T>
class RTree {
   protected:
    virtual const_iterator queryImpl(const std::function<bool(const T&)>& pred) const = 0;

   public:
    using iterator = std::any_iterator<T&, std::input_iterator_tag>;
    using const_iterator = std::any_iterator<const T&, std::input_iterator_tag>;

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

    virtual iterator begin() = 0;
    virtual iterator end() = 0;

    virtual const_iterator begin() const = 0;
    virtual const_iterator end() const = 0;

    template <typename Predicate>
    iterator query(Predicate pred) {
        return queryImpl(pred);
    }

    template <typename Predicate>
    const_iterator query(Predicate pred) const {
        return queryImpl(pred);
    }

    virtual unsigned int size() const = 0;

    virtual bool empty() const = 0;
    virtual void clear() = 0;
};