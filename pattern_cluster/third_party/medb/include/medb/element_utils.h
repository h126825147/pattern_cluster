/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef ELEMENT_UTILS_H
#define ELEMENT_UTILS_H

#include "medb/element.h"

// 特化 std::hash 模板，为 element 类型定义具体的哈希函数实现
namespace std {
template <>
class hash<medb2::Element> {
public:
    size_t operator()(const medb2::Element &element) const
    {
        size_t hashValue = std::hash<uintptr_t>()(element.data_);
        return hashValue;
    }
};

}  // namespace std
#endif  // ELEMENT_UTILS_H