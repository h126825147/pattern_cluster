/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_HASH_H
#define MEDB_HASH_H

#include <functional>
#include "medb/base_utils.h"

namespace medb2 {
// 源自 boost 库中的 HashCombine
template <class Type>
inline void HashCombine(size_t &seed, Type value)
{
    // 魔数 0x9e3779b9 是 2^32 的黄金分割值；左挪 6 位和右挪 2 位是单纯的比特位的洗牌，能打散避免在比特位上聚集的数
    seed ^= std::hash<Type>()(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <class ShapeType>
class MEDB_API PtrHash {
public:
    size_t operator()(const ShapeType *ptr) const;
};

template <class ShapeType>
class MEDB_API PtrEqual {
public:
    bool operator()(const ShapeType *ptr1, const ShapeType *ptr2) const;
};
}  // namespace medb2

#endif  // MEDB_HASH_H
