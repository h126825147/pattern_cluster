/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_ALLOCATOR_H
#define MEDB_ALLOCATOR_H

#include <cstddef>
#include <cstdint>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "medb/base_utils.h"

namespace medb2 {

/**
 * @brief 堆区申请一块内存
 *
 * @param size 申请的内存大小
 * @return void* 返回申请的内存，需由medb_free释放
 */
void MEDB_API *medb_malloc(size_t size);

/**
 * @brief 释放medb_malloc申请的内存
 *
 * @param p 要释放的内存
 */
void MEDB_API medb_free(void *p);

/**
 * @brief 使用medb_malloc实现的new
 *
 * @tparam T 需要new的类型
 * @tparam Args 构造参数类型
 * @param args 构造参数
 * @return T* 对象指针
 */
template <typename T, typename... Args>
T *medb_new(Args &&...args)
{
    return new (medb_malloc(sizeof(T))) T(std::forward<Args>(args)...);
}

/**
 * @brief 使用medb_free实现的delete
 *
 * @tparam T 对象类型
 * @param ptr 需要释放的指针
 */
template <typename T>
void medb_delete(T *ptr)
{
    ptr->~T();
    medb_free(ptr);
}

void MEDB_API medb_malloc_trim();

template <typename T>
class Allocator {
public:
    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type *;
    using const_pointer = value_type const *;
    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap = std::true_type;
    using is_always_equal = std::true_type;

    template <class U, class... Args>
    void construct(U *p, Args &&...args)
    {
        ::new (p) U(std::forward<Args>(args)...);
    }
    template <class U>
    void destroy(U *p) noexcept
    {
        p->~U();
    }

    size_type max_size() const noexcept { return (PTRDIFF_MAX / sizeof(value_type)); }
    pointer address(value_type &x) const { return static_cast<pointer>(&x); }
    const_pointer address(const value_type &x) const { return static_cast<const_pointer>(&x); }

    template <class U>
    struct rebind {
        using other = Allocator<U>;
    };

    Allocator() noexcept = default;
    Allocator(const Allocator &) noexcept = default;
    Allocator &operator=(const Allocator &) noexcept = default;

    template <class U>
    explicit Allocator(const Allocator<U> &) noexcept
    {
    }

    Allocator select_on_container_copy_construction() const { return *this; }

    void deallocate(void *p, size_type) { medb2::medb_free(p); }
    [[nodiscard]] value_type *allocate(size_type count)
    {
        return static_cast<value_type *>(medb2::medb_malloc(count * sizeof(value_type)));
    }
    [[nodiscard]] value_type *allocate(size_type count, const void *) { return allocate(count); }
};

template <typename T>
class NoDefaultInitAllocator : public Allocator<T> {
public:
    using Allocator<T>::Allocator;

    template <class U>
    struct rebind {
        using other = NoDefaultInitAllocator<U>;
    };

    // 不执行 T 的默认构造
    template <class U>
    void construct(U * /* p */)
    {
    }
};

template <class T1, class T2>
bool operator==(const Allocator<T1> &, const Allocator<T2> &) noexcept
{
    return true;
}
template <class T1, class T2>
bool operator!=(const Allocator<T1> &, const Allocator<T2> &) noexcept
{
    return false;
}

template <typename T>
using MeList = std::list<T, Allocator<T>>;

template <typename T>
using MeVector = std::vector<T, Allocator<T>>;

template <typename K, typename V, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
using MeUnorderedMap = std::unordered_map<K, V, Hash, Pred, Allocator<std::pair<const K, V>>>;

template <typename K, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
using MeUnorderedSet = std::unordered_set<K, Hash, Pred, Allocator<K>>;

template <typename K, typename V, typename Cmp = std::less<K>>
using MeMap = std::map<K, V, Cmp, Allocator<std::pair<const K, V>>>;

template <typename K, typename V, typename Cmp = std::less<K>>
using MeMultimap = std::multimap<K, V, Cmp, Allocator<std::pair<const K, V>>>;

template <typename K, typename Cmp = std::less<K>>
using MeSet = std::set<K, Cmp, Allocator<K>>;
template <typename K>
using MeDeque = std::deque<K, Allocator<K>>;
template <typename K, typename Seq = MeDeque<K>>
using MeQueue = std::queue<K, Seq>;

template <typename K>
using MeStack = std::stack<K, MeVector<K>>;

#define USE_MEDB_OPERATOR_NEW_DELETE                                                          \
    void *operator new(size_t size) { return medb2::medb_malloc(size); }                      \
    void *operator new(size_t size, void *ptr) noexcept { return ::operator new(size, ptr); } \
    void *operator new[](size_t size) { return medb2::medb_malloc(size); }                    \
    void operator delete(void *p) { return medb2::medb_free(p); }                             \
    void operator delete[](void *p) { return medb2::medb_free(p); }

}  // namespace medb2

#endif  // MEDB_ALLOCATOR_H
