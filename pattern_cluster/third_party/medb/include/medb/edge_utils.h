/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef MEDB_EDGE_UTILS_H
#define MEDB_EDGE_UTILS_H
#include "medb/box_utils.h"
#include "medb/edge.h"
#include "medb/vector_utils.h"

namespace std {
template <class CoordType>
class hash<medb2::Edge<CoordType>> {
public:
    // 将四个不同的坐标值的哈希码混合在一起，生成唯一的哈希码，用于表示一个 medb2::Edge 对象的哈希值
    size_t operator()(const medb2::Edge<CoordType> &edge) const
    {
        size_t seed = 0;
        medb2::HashCombine(seed, edge.BeginPoint());
        medb2::HashCombine(seed, edge.EndPoint());
        return seed;
    }
};
}  // namespace std

namespace medb2 {

/**
 * @brief 计算线段 e1 在线段 e2 上的投影长度，结果的取值范围为 [0, length(e2)]
 *
 * @param e1 线段1
 * @param e2 线段2
 * @return double 返回线段 e1 在线段 e2 上的投影长度
 */
inline double GetProjectionDistace(const EdgeI &e1, const EdgeI &e2)
{
    Point<int64_t> p11(e1.BeginPoint());
    Point<int64_t> p12(e1.EndPoint());
    Point<int64_t> p21(e2.BeginPoint());
    Point<int64_t> p22(e2.EndPoint());
    Vector<int64_t> vec2 = p22 - p21;
    Vector<int64_t> vec_begin1 = p11 - p21;
    Vector<int64_t> vec_begin2 = p12 - p21;

    double len2 = VectorLength(vec2);
    double projection_max = static_cast<double>(DotProduct(vec2, vec_begin1)) / len2;
    if (DoubleLessEqual(projection_max, 0)) {
        return 0;
    }
    double projection_min = static_cast<double>(DotProduct(vec2, vec_begin2)) / len2;
    if (DoubleGreaterEqual(projection_min, len2)) {
        return 0;
    }
    projection_max = std::clamp(projection_max, 0.0, len2);
    projection_min = std::clamp(projection_min, 0.0, len2);
    return projection_max - projection_min;
}

/**
 * @brief 判断两条线段是否相交
 *
 * @param e1 线段1
 * @param e2 线段2
 * @return bool 若两条线段相交（包含点相交），则返回 true，否则返回 false
 */
inline bool IsIntersect(const EdgeI &e1, const EdgeI &e2)
{
    if (!IsIntersect(BoxI(e1.BeginPoint(), e1.EndPoint()), BoxI(e2.BeginPoint(), e2.EndPoint()))) {
        return false;
    }
    Point<int64_t> e1_begin(e1.BeginPoint());
    Point<int64_t> e1_end(e1.EndPoint());
    Point<int64_t> e2_begin(e2.BeginPoint());
    Point<int64_t> e2_end(e2.EndPoint());
    int64_t cross_product1 = CrossProduct(e1_end - e1_begin, e2_begin - e1_begin);
    int64_t cross_product2 = CrossProduct(e1_end - e1_begin, e2_end - e1_begin);
    int64_t cross_product3 = CrossProduct(e2_end - e2_begin, e1_begin - e2_begin);
    int64_t cross_product4 = CrossProduct(e2_end - e2_begin, e1_end - e2_begin);
    if ((cross_product1 > 0 && cross_product2 > 0) || (cross_product1 < 0 && cross_product2 < 0) ||
        (cross_product3 > 0 && cross_product4 > 0) || (cross_product3 < 0 && cross_product4 < 0)) {
        return false;
    }
    return true;
}

/**
 * @brief 获取一个点到一个线段上的最短距离的平方
 *
 * @param point 点
 * @param edge 线段
 * @return double 返回一个点到一个线段上的最短距离的平方
 */
inline double GetMinDistanceSquare(const PointI &point, const EdgeI &edge)
{
    Point<int64_t> edge_point1(edge.BeginPoint());
    Point<int64_t> edge_point2(edge.EndPoint());
    Vector<int64_t> vec = edge_point2 - edge_point1;
    int64_t projection = DotProduct(Point<int64_t>(point) - edge_point1, vec);
    int64_t vec_len_square = DotProduct(vec, vec);
    if (projection <= 0) {
        Vector<int64_t> vec2 = Point<int64_t>(point) - edge_point1;
        return static_cast<double>(DotProduct(vec2, vec2));
    } else if (projection >= vec_len_square) {
        Vector<int64_t> vec2 = Point<int64_t>(point) - edge_point2;
        return static_cast<double>(DotProduct(vec2, vec2));
    } else {
        Vector<int64_t> vec2 = Point<int64_t>(point) - edge_point1;
        return static_cast<double>(DotProduct(vec2, vec2)) -
               static_cast<double>(projection * projection) / static_cast<double>(vec_len_square);
    }
}

/**
 * @brief 获取两个线段上任意两点间的最短距离
 *
 * @param e1 线段1
 * @param e2 线段2
 * @return double 返回两个线段上任意两点间的最短距离
 */
inline double GetMinDistance(const EdgeI &e1, const EdgeI &e2)
{
    // 判断两条线段是否相交
    if (IsIntersect(e1, e2)) {
        return 0;
    }
    double min_distance_square =
        std::min({GetMinDistanceSquare(e1.BeginPoint(), e2), GetMinDistanceSquare(e1.EndPoint(), e2),
                  GetMinDistanceSquare(e2.BeginPoint(), e1), GetMinDistanceSquare(e2.EndPoint(), e1)});
    return std::sqrt(min_distance_square);
}

}  // namespace medb2

#endif