/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_UTILS_H
#define MEDB_UTILS_H

#include "medb/linked_edges.h"
#include "medb/path.h"
#include "medb/point.h"
#include "medb/point_utils.h"
#include "medb/polygon.h"
#include "medb/polygon_utils.h"
#include "medb/vector_utils.h"

namespace medb2 {
template <class CoordType>
class Box;
template <class CoordType>
class Ring;
template <class ShapeType>
class ShapeRepetition;

template <class CoordType>
inline bool IsClockWise(const Ring<CoordType> &ring)
{
    const auto &points = ring.Raw();
    return IsClockWise(points.data(), points.size(), ring.Flag().compress_type);
}

template <class ShapeType>
inline auto const &GetBoundingBox(const ShapeType &shape)
{
    return shape.BoundingBox();
}

// ShapeRepetition 中没有 BoundingBox 的缓存，因此不能返回引用
template <class ShapeType>
inline auto const GetBoundingBox(const ShapeRepetition<ShapeType> &shape)
{
    return shape.BoundingBox();
}

template <class Iter>
inline auto GetBoundingBox(Iter first, Iter end)
{
    auto box = GetBoundingBox(*first);
    ++first;
    std::for_each(first, end, [&box](auto const &shape) { BoxUnion(box, GetBoundingBox(shape)); });
    return box;
}

template <class CoordType>
inline Polygon<CoordType> BoxToPolygon(const Box<CoordType> &box)
{
    MeVector<Point<CoordType>> poly;
    poly.reserve(kRectanglePointSize);
    poly.emplace_back(box.BottomLeft());
    poly.emplace_back(box.BottomLeft().X(), box.TopRight().Y());
    poly.emplace_back(box.TopRight());
    poly.emplace_back(box.TopRight().X(), box.BottomLeft().Y());
    return Polygon<CoordType>(poly.data(), kRectanglePointSize);
}

/**
 * @brief 对大于等于阈值点的polygon进行分割处理
 *
 * @param in 输入的多边形
 * @param out 输出被拆分的多边形集合
 * @param threshold 切割点数量的阈值, 默认8000
 */
void MEDB_API SplitPolygon(const PolygonI &in, std::vector<PolygonI> &out, int threshold = 8000);

/**
 * @brief 校验输入的多边形中是否存在空多边形 或者 非压缩点小于3 或者 压缩点小于2的不合法多边形，
 * 如果存在返回true并打印错误日志
 *
 * @param polys 输入的多边形集合
 * @param compress_type 多边形压缩类型
 * @return true: 多边形集合不合法 false: 不存在
 */
bool MEDB_API CheckPolygonsInvalid(const std::vector<PolygonPtrDataI> &polys, ManhattanCompressType compress_type);

template <class CoordType>
inline LinkedEdges<CoordType> PolygonToLinkedEdges(const Polygon<CoordType> &polygon)
{
    LinkedEdges<CoordType> linked_edges(polygon.Points(), IsClockWise(polygon));
    return linked_edges;
}

template <class CoordType>
inline Polygon<CoordType> LinkedEdgesToPolygon(const LinkedEdges<CoordType> &linked_edges)
{
    Polygon<CoordType> polygon;
    const auto &edge_list = linked_edges.EdgeList();
    if (edge_list.empty()) {
        return polygon;
    }

    std::vector<Point<CoordType>> points;
    for (auto iter = edge_list.begin(); iter != edge_list.end(); ++iter) {
        points.emplace_back(iter->BeginPoint());
    }
    polygon.SetPoints(points);
    return polygon;
}

inline PolygonDataI ToPolygonData(std::vector<PointI> &&points, const ShapeManhattanType type)
{
    // 拆洞
    MeVector<RingI> split_result;
    SplitHoles(points, type, split_result);
    PolygonDataI poly_data;
    if (split_result.empty()) {
        poly_data.emplace_back(std::move(points));
    } else {
        for (auto &ring : split_result) {
            poly_data.emplace_back(std::move(ring.TakeData()));
        }
    }
    return poly_data;
}

// 仅支持 BoxI/PolygonI/PathI 三种类型，其它类型会编译报错(无 return 语句)
template <class ShapeType>
inline PolygonDataI ToPolygonData(ShapeType &&shape)
{
    using T = std::remove_cv_t<std::remove_reference_t<ShapeType>>;
    if constexpr (std::is_same_v<BoxI, T>) {
        std::vector<PointI> points;
        points.reserve(4);  // box 有 4 个点
        points.emplace_back(shape.BottomLeft());
        points.emplace_back(shape.BottomLeft().X(), shape.TopRight().Y());
        points.emplace_back(shape.TopRight());
        points.emplace_back(shape.TopRight().X(), shape.BottomLeft().Y());
        return PolygonDataI({std::move(points)});
    }

    if constexpr (std::is_same_v<PolygonI, T>) {
        return ToPolygonData(shape.Points(), shape.Flag().manhattan_type);
    }

    if constexpr (std::is_same_v<PathI, T>) {
        auto &polygon = shape.ToPolygon();
        return ToPolygonData(polygon.Points(), polygon.Flag().manhattan_type);
    }
}

// 需要保证这三个点在外环上是顺时针或在内环上是逆时针排列
inline CornerType GetCornerType(const PointI &first, const PointI &middle, const PointI &last)
{
    Point<int64_t> p1(first);
    Point<int64_t> p2(middle);
    Point<int64_t> p3(last);
    int64_t cross_product = CrossProduct(p2 - p1, p3 - p2);
    if (cross_product > 0) {
        return CornerType::kConcave;
    } else if (cross_product < 0) {
        return CornerType::kConvex;
    } else {
        return CornerType::kStraight;
    }
}
}  // namespace medb2
#endif
