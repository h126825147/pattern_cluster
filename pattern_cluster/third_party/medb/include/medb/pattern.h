/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_PATTERN_H
#define MEDB_PATTERN_H

#include <string>

namespace medb2 {

/**
 * @brief Pattern是版图进行pattern match的基本单元，包含id和一组polygon
 */
class Pattern {
public:
    using IdType = std::string;
    Pattern() = default;
    ~Pattern() = default;

    /**
     * @brief 根据给出的id和polygons构造一个Pattern对象
     * @param id Pattern对象的id
     */
    explicit Pattern(const IdType &id) : id_(id) {}

    /**
     * @brief 获取pattern的id
     * @return pattern id值
     */
    const IdType &Id() const { return id_; }

private:
    IdType id_;
};
}  // namespace medb2
#endif  // MEDB_PATTERN_H
