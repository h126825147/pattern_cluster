/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_LAYER_H
#define MEDB_LAYER_H

#include <limits>
#include <string>
#include "medb/base_utils.h"

namespace medb2 {

class MEDB_API Layer {
public:
    Layer() = default;
    Layer(const Layer &l) = default;
    Layer(Layer &&l) = default;
    Layer &operator=(const Layer &l) = default;
    Layer &operator=(Layer &&l) = default;
    ~Layer() = default;

    Layer(uint32_t id, uint32_t type) : id_(id), type_(type) {}

    bool operator==(const Layer &other) const { return id_ == other.id_ && type_ == other.type_; }

    bool operator!=(const Layer &other) const { return !(*this == other); }

    bool operator<(const Layer &other) const { return id_ < other.id_ || (id_ == other.id_ && type_ < other.type_); }

    uint32_t Id() const { return id_; }

    uint32_t Type() const { return type_; }

    uint64_t Key() const { return key_; }

    std::string ToString() const { return "{" + std::to_string(id_) + ", " + std::to_string(type_) + "}"; }

private:
    union {
        struct {
            uint32_t id_;
            uint32_t type_;
        };
        uint64_t key_{0};
    };
};

}  // namespace medb2

namespace std {

/**
 * @brief 添加 Layer 对象的哈希值实现，Cell 类有依赖此功能
 *
 */
template <>
class hash<medb2::Layer> {
public:
    size_t operator()(const medb2::Layer &l) const { return hash<uint64_t>()(l.Key()); }
};
}  // namespace std

#endif  // MEDB_LAYER_H
