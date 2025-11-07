/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef POLYGON_UTILS_H
#define POLYGON_UTILS_H

#include "box_utils.h"
#include "hash.h"
#include "point_utils.h"
#include "polygon.h"

namespace std {
// Polygon 中没有 BoundingBox 缓存时，可使用该 hash 函数
template <>
class hash<medb2::PolygonI> {
public:
    size_t operator()(const medb2::PolygonI &poly) const
    {
        size_t seed = 0;
        medb2::HashCombine(seed, poly.PointSize());
        medb2::HashCombine(seed, poly.Flag().compress_type);
        medb2::HashCombine(seed, poly.Flag().manhattan_type);
        // hash 计算最多只计算 polygon 的前 20 个点，大部分情况下足够标识一个 polygon
        int32_t size = std::min(20, static_cast<int32_t>(poly.PointSize()));
        auto point_data = poly.PointData();
        for (int32_t i = 0; i < size; ++i) {
            medb2::HashCombine(seed, point_data[i]);
        }
        return seed;
    }
};
}  // namespace std

namespace medb2 {
// Polygon 中有 BoundingBox 缓存时，可使用该 hash 函数
template <>
class PtrHash<PolygonI> {
public:
    size_t operator()(const PolygonI *ptr) const { return std::hash<BoxI>()(ptr->BoundingBox()); }
};

template <>
class PtrEqual<PolygonI> {
public:
    size_t operator()(const PolygonI *ptr1, const PolygonI *ptr2) const { return *ptr1 == *ptr2; }
};

template <class CoordType>
inline bool IsClockWise(const Polygon<CoordType> &poly)
{
    return Integrate(poly.PointData(), poly.PointSize(), poly.Flag().compress_type) < 0;
}

}  // namespace medb2

#endif  // POLYGON_UTILS_H