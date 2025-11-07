/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_POINT_UTILS_H
#define MEDB_POINT_UTILS_H

#include <algorithm>
#include <memory>
#include "enums.h"
#include "medb/box.h"
#include "medb/point.h"
#include "medb/vector_utils.h"

namespace std {
template <class CoordType>
class hash<medb2::Point<CoordType>> {
public:
    size_t operator()(const medb2::Point<CoordType> &point) const
    {
        size_t seed = 0;
        medb2::HashCombine(seed, point.X());
        medb2::HashCombine(seed, point.Y());
        return seed;
    }
};
}  // namespace std

namespace medb2 {
// 对一个表示manhattan图形的常规点集，进行压缩，要求input一定是一个表示manhattan图形的常规点集
template <class CoordType>
void CompressManhattanPoints(const Point<CoordType> *input, uint32_t input_size, Point<CoordType> *output)
{
    if (input_size < kRingMinPointCount) {
        return;
    }
    for (int i = 0, j = 0; i < static_cast<int>(input_size); i += 2, ++j) {  // 2 表示隔一个点取一个点
        output[j] = input[i];
    }
}

template <class CoordType>
void CompressManhattanPoints(const std::vector<Point<CoordType>> &input, std::vector<Point<CoordType>> &output)
{
    output.resize(input.size() / 2);
    CompressManhattanPoints<CoordType>(input.data(), static_cast<uint32_t>(input.size()), output.data());
}

// 对一个manhattan压缩的点集进行解压缩，即还原成常规点集，要求入参input一定是一个manhattan压缩点集
// OutContainer 为 std::vector<Point<CoordType>> 或者 MeVector<Point<CoordType>>
template <class CoordType, class OutContainer>
void DecompressManhattanPoints(const Point<CoordType> *input, uint32_t input_size,
                               const ManhattanCompressType &compress_type, OutContainer &output)
{
    output.clear();
    if (input_size < kCompressRingMinPointCount) {
        return;
    }
    int size = static_cast<int>(input_size);
    bool horizontal_first = compress_type == kCompressH;
    // 压缩方式是间隔存储，隔一个点存一个点，因此原始点数是压缩点数的2倍，以下算法中的“2”均与此相关。
    int output_size = size * 2;
    output.reserve(output_size);
    int compressh = horizontal_first ? 1 : 0;
    int compressv = horizontal_first ? 0 : 1;
    for (int i = 0; i < size; ++i) {
        output.emplace_back(input[i]);
        int index_x = i + compressh;
        int index_y = i + compressv;
        if (index_x >= size) {
            index_x = 0;
        }
        if (index_y >= size) {
            index_y = 0;
        }
        output.emplace_back(Point(input[index_x].X(), input[index_y].Y()));
    }
}

template <class CoordType, class OutContainer>
void DecompressManhattanPoints(const std::vector<Point<CoordType>> &input, const ManhattanCompressType &compress_type,
                               OutContainer &output)
{
    DecompressManhattanPoints<CoordType>(input.data(), static_cast<uint32_t>(input.size()), compress_type, output);
}

template <class CoordType>
bool IsBox(const Point<CoordType> *points, uint32_t points_size)
{
    if (points_size != 4) {
        return false;
    }

    int i = static_cast<int>(points_size) - 1;
    int j = 0;
    while (j < static_cast<int>(points_size)) {
        if (points[i].X() != points[j].X() && points[i].Y() != points[j].Y()) {
            return false;
        }
        i = j;
        ++j;
    }

    return true;
}

template <class CoordType>
bool IsCollinear(const Point<CoordType> &p1, const Point<CoordType> &p2, const Point<CoordType> &p3)
{
    auto result = CrossProduct(SafeSub(p2, p1), SafeSub(p3, p2));
    return CoordEqual<decltype(result)>(result, 0);
}

template <class CoordType>
void RemoveDuplicateAndCollinearPoint(Point<CoordType> *points, uint32_t &points_size)
{
    if (points_size < kRingMinPointCount) {
        return;
    }

    // Remove duplicate
    Point<CoordType> *last = std::unique(points, points + points_size);
    points_size = static_cast<uint32_t>(last - points);
    if (points[0] == points[points_size - 1]) {
        points_size--;
    }

    if (points_size < kRingMinPointCount) {
        points_size = 0;
        return;
    }

    // Remove colinear
    bool found_colinear = true;
    while (found_colinear && points_size >= kRingMinPointCount) {
        found_colinear = false;
        Point<CoordType> *pre = points + (points_size - 1);
        Point<CoordType> *mid = points;
        Point<CoordType> *next = points + 1;
        uint32_t count = 0;
        while (next < points + points_size) {
            if (IsCollinear(*pre, *mid, *next)) {
                count++;
                found_colinear = true;
            } else {
                pre = mid;
                mid++;
            }
            *mid = *next;
            next++;
        }
        next = points;
        if (IsCollinear(*pre, *mid, *next)) {
            count++;
            found_colinear = true;
        }
        points_size -= count;
    }

    if (points_size < kRingMinPointCount) {
        points_size = 0;
        return;
    }
}

template <class PointsContainer>
void RemoveDuplicateAndCollinearPoint(PointsContainer &points)
{
    uint32_t size = static_cast<uint32_t>(points.size());
    RemoveDuplicateAndCollinearPoint(points.data(), size);
    points.resize(size);
}

template <class CoordType>
void FilterPath(Point<CoordType> *points, uint32_t &points_size)
{
    // Remove duplicate
    Point<CoordType> *last = std::unique(points, points + points_size);
    points_size = static_cast<uint32_t>(last - points);

    if (points_size < kNeedFilterMinPointCount) {
        return;
    }

    // Remove colinear
    Point<CoordType> *pre = points;
    Point<CoordType> *mid = points + 1;
    Point<CoordType> *next = points + 2;
    uint32_t count = 0;
    while (next < points + points_size) {
        auto result = CrossProduct(*mid - *pre, *next - *mid);
        if (CoordEqual<decltype(result)>(result, 0)) {
            count++;
        } else {
            pre = mid;
            mid++;
        }
        *mid = *next;
        next++;
    }
    points_size -= count;
}

template <class CoordType>
void FilterPath(std::vector<Point<CoordType>> &points)
{
    uint32_t size = static_cast<uint32_t>(points.size());
    FilterPath(points.data(), size);
    points.resize(size);
}

/**
 * @brief 获取点集形式的多边形外边框
 *
 * @param points 多边形的起点
 * @param point_size 多边形点的个数
 * @return 返回box的坐下点和右上点
 */
template <class CoordType>
Box<CoordType> GetBoundingBox(const Point<CoordType> *points, size_t point_size)
{
    if (point_size == 0) {
        return Box<CoordType>(0, 0, 0, 0);
    }
    CoordType left = std::numeric_limits<CoordType>::max();
    CoordType bottom = std::numeric_limits<CoordType>::max();
    CoordType right = std::numeric_limits<CoordType>::lowest();
    CoordType top = std::numeric_limits<CoordType>::lowest();
    for (int i = 0; i < static_cast<int>(point_size); ++i) {
        auto &point = points[i];
        left = std::min(left, point.X());
        right = std::max(right, point.X());
        bottom = std::min(bottom, point.Y());
        top = std::max(top, point.Y());
    }

    return Box<CoordType>(left, bottom, right, top);
}

/**
 * @brief 获取点集形式的多边形外边框
 *
 * @param hull 多边形的外环
 * @return 返回box的坐下点和右上点
 */
template <class CoordType>
Box<CoordType> GetBoundingBox(const std::vector<Point<CoordType>> &hull)
{
    return GetBoundingBox(hull.data(), hull.size());
}

/**
 * @brief 获取点集形式的多边形外边框
 *
 * @param hull 多边形的外环
 * @return 返回box的坐下点和右上点
 */
template <class CoordType>
Box<CoordType> GetBoundingBox(const MeVector<Point<CoordType>> &hull)
{
    return GetBoundingBox(hull.data(), hull.size());
}

/**
 * @brief 获取点集形式的多边形的面积
 *
 * @param points 多边形的起点
 * @param point_size 多边形点的个数
 * @param compress_type 点集的压缩方式
 * @return 返回多边形的面积
 */
template <class CoordType>
double Area(const Point<CoordType> *points, size_t point_size, const ManhattanCompressType &compress_type)
{
    return std::abs(Integrate(points, point_size, compress_type));
}

/**
 * @brief 对点集形式的多边形进行积分，若点集为逆时针则结果为正值，反之为负值
 *
 * @param points 多边形的起点
 * @param point_size 多边形点的个数
 * @param compress_type 点集的压缩方式
 * @return 返回多边形的积分，若点集为逆时针则结果为正值，反之为负值
 */
template <class CoordType>
double Integrate(const Point<CoordType> *points, size_t point_size, const ManhattanCompressType &compress_type)
{
    // 无压缩的点集
    if (compress_type == kNoCompress) {
        if (point_size < kRingMinPointCount) {
            return 0;
        }
        // 输入顶点按逆时针排列时，s为正值；否则s为负值
        double s = static_cast<double>(points[0].Y()) *
                   (static_cast<double>(points[point_size - 1].X()) - static_cast<double>(points[1].X()));
        for (int i = 1; i < static_cast<int>(point_size); ++i) {
            s += static_cast<double>(points[i].Y()) *
                 (static_cast<double>(points[i - 1].X()) - static_cast<double>(points[(i + 1) % point_size].X()));
        }
        return s / 2.0;  // 2.0 表示求三角形面积除以 2.0
    }

    // 有压缩的点集
    if (point_size < kCompressRingMinPointCount) {
        return 0;
    }
    double s = 0;
    int end_index = static_cast<int>(point_size - 1);
    if (compress_type == kCompressH) {
        for (int i = 0; i < end_index; ++i) {
            s += static_cast<double>(points[i].Y()) *
                 (static_cast<double>(points[i].X()) - static_cast<double>(points[i + 1].X()));
        }
        s += static_cast<double>(points[end_index].Y()) *
             (static_cast<double>(points[end_index].X()) - static_cast<double>(points[0].X()));
    } else {
        for (int i = 0; i < end_index; ++i) {
            s += static_cast<double>(points[i].X()) *
                 (static_cast<double>(points[i + 1].Y()) - static_cast<double>(points[i].Y()));
        }
        s += static_cast<double>(points[end_index].X()) *
             (static_cast<double>(points[0].Y()) - static_cast<double>(points[end_index].Y()));
    }

    return s;
}

/**
 * @brief 求多边形的顺逆时针
 *
 * @param points 多边形的起点
 * @param point_size 多边形点的个数
 * @param compress_type 点集的压缩方式
 * @return bool true 表示顺时针，false 表示逆时针
 */
template <class CoordType>
inline bool IsClockWise(const Point<CoordType> *points, size_t point_size, const ManhattanCompressType &compress_type)
{
    return Integrate(points, point_size, compress_type) < 0;
}

/**
 * @brief 获取轮廓的类型，这里的轮廓可能是多边形的外环，也可能是多边形里面的某个内环（即洞）
 *
 * @param Points 围成轮廓的点
 * @return ShapeManhattanType 轮廓的类型
 */
template <class CoordType>
inline ShapeManhattanType GetPointsType(const Point<CoordType> *points, uint32_t point_size)
{
    if (point_size < kRingMinPointCount) {
        return ShapeManhattanType::kUnknown;
    }
    bool is_manhattan = true;
    const Point<CoordType> *pt_prev = points + point_size - 1;
    const Point<CoordType> *pt_curr = points;
    for (uint32_t i = 0; i < point_size; ++i) {
        if (!CoordEqual(pt_prev->X(), pt_curr->X()) && !CoordEqual(pt_prev->Y(), pt_curr->Y())) {
            is_manhattan = false;
            auto dx = static_cast<typename CoordTraits<CoordType>::OverFlowType>(pt_prev->X()) - pt_curr->X();
            auto dy = static_cast<typename CoordTraits<CoordType>::OverFlowType>(pt_prev->Y()) - pt_curr->Y();
            if (!CoordEqual(std::abs(dx), std::abs(dy))) {
                return ShapeManhattanType::kAnyAngle;
            }
        }
        pt_prev = pt_curr;
        pt_curr++;
    }
    if (is_manhattan) {
        return ShapeManhattanType::kManhattan;
    } else {
        return ShapeManhattanType::kOctangular;
    }
}

/**
 * @brief 获取多边形的类型
 *
 * 若多边形的外环、内环中有一个为 ORDINARY ， 则多边形的类型为 ShapeManhattanType::kAnyAngle（任意多边形）；
 * 否则，若多边形的外环、内环中有一个为 ShapeManhattanType::kOctangular，则多边形的类型为
 * ShapeManhattanType::kOctangular（半曼哈顿）； 否则，多边形的类型为 ShapeManhattanType::kManhattan（曼哈顿）；
 *
 * @param polygon 指定要获取类型的多边形
 * @return ShapeManhattanType 多边形的类型
 */
template <class CoordType>
inline ShapeManhattanType GetPolygonType(const std::vector<std::vector<Point<CoordType>> *> &polygon)
{
    bool is_manhattan = true;
    for (const auto &ring : polygon) {
        ShapeManhattanType type = ShapeManhattanType::kAnyAngle;
        type = GetPointsType(ring->data(), static_cast<uint32_t>(ring->size()));
        if (type == ShapeManhattanType::kAnyAngle) {
            return ShapeManhattanType::kAnyAngle;
        }

        if (type == ShapeManhattanType::kOctangular) {
            is_manhattan = false;
        }
    }

    return (is_manhattan ? ShapeManhattanType::kManhattan : ShapeManhattanType::kOctangular);
}

/**
 * @brief 获取多边形集合的类型
 *
 * 若集合中有一个多边形的类型为 ORDINARY ， 则多边形集合的类型为 ORDINARY（任意多边形）；
 * 否则若集合中有个一个多边形的类型为 ShapeManhattanType::kOctangular，则多边形集合的类型为
 * ShapeManhattanType::kOctangular（半曼哈顿）； 否则多边形集合的类型为 ShapeManhattanType::kManhattan（曼哈顿）；
 *
 * @param polygons 入参，多边形的集合
 * @return ShapeManhattanType 多边形集合类型
 */
template <class CoordType>
inline ShapeManhattanType GetPolygonVecType(const std::vector<std::vector<std::vector<Point<CoordType>> *>> &polygons)
{
    bool is_manhattan = true;
    for (const auto &poly : polygons) {
        ShapeManhattanType type = ShapeManhattanType::kAnyAngle;
        type = GetPolygonType(poly);
        if (type == ShapeManhattanType::kAnyAngle) {
            return ShapeManhattanType::kAnyAngle;
        }

        if (type == ShapeManhattanType::kOctangular) {
            is_manhattan = false;
        }
    }

    return (is_manhattan ? ShapeManhattanType::kManhattan : ShapeManhattanType::kOctangular);
}

/**
 * @brief 翻转点序
 *
 * @param poly 入参/出参， 需要翻转点序的多边形
 */
template <class PointType>
inline void ReversePoly(std::vector<std::vector<PointType>> &poly)
{
    for (auto &ring : poly) {
        std::reverse(ring.begin(), ring.end());
    }
}

/**
 * @brief 翻转点序
 *
 * @param polys 入参/出参， 需要翻转点序的多边形点集集合
 */
template <class PointType>
inline void ReversePoints(std::vector<std::vector<std::vector<PointType>>> &polys)
{
    for (auto &poly : polys) {
        ReversePoly(poly);
    }
}

/**
 * @brief 把 RingDataI 当个环按照左下角为起始点
 *
 * @param ring 入参，当个环
 */
inline void RingSetLeftBottom(RingDataI &ring)  // cppcheck-suppress constParameterReference
{
    if (ring.empty()) {
        return;
    }
    int idx = 0;
    int l = std::numeric_limits<int32_t>::max();
    int b = std::numeric_limits<int32_t>::max();
    for (int i = 0; i < static_cast<int>(ring.size()); ++i) {
        if (ring[i].X() < l || (ring[i].X() == l && ring[i].Y() < b)) {
            l = ring[i].X();
            b = ring[i].Y();
            idx = i;
        }
    }

    std::rotate(ring.begin(), ring.begin() + idx, ring.end());
}

/**
 * @brief 把 PolygonPtrDataI 的点集按照左下角为起始点
 *
 * @param polygons 入参，多边形集合
 */
inline void RotateSetLeftBottom(std::vector<PolygonPtrDataI> &polygons)
{
    if (polygons.empty()) {
        return;
    }

    for (auto &polygon : polygons) {
        for (auto &ring_ptr : polygon) {
            auto &ring = *ring_ptr;
            RingSetLeftBottom(ring);
        }
    }
}

/**
 * @brief 检查 polygon 是否包含这个 point, point 在 polygon 的边界上不算包含
 *
 * @param polygon polygon
 * @param point point
 */
static inline bool ContainPoint(const RingDataI &ring, const PointI &point)
{
    // 对于所有x轴区间包含point_x的线段，判断在point上方的边的个数。奇数个为在polygon内，偶数个为polygon外
    int sum = 0;
    if (ring.size() < kRingMinPointCount) {
        return false;
    }
    int size = static_cast<int>(ring.size());
    for (int idx = 0; idx < size; ++idx) {
        PointI start = ring[idx];
        PointI end = ring[(idx + 1) % size];
        if (start.X() > end.X()) {
            std::swap(start, end);
        }
        if (start.X() <= point.X() && end.X() > point.X()) {
            int64_t cross_product = CrossProduct(end - start, point - start);
            if (cross_product == 0) {
                return false;
            } else if (cross_product > 0) {
                ++sum;
            }
        }
    }
    return sum % 2 == 1;
}

/**
 * @brief 把 PolygonDataI 类型的多边形集合转换成 PolygonPtrDataI 类型的多边形集合
 *
 * @param src 入参，PolygonDataI 类型的多边形集合
 * @param dst 出参，PolygonPtrDataI 类型的多边形集合
 */
inline void PolygonToPtr(std::vector<PolygonDataI> &src, std::vector<PolygonPtrDataI> &dst)
{
    dst.clear();
    for (auto &poly : src) {
        PolygonPtrDataI tmp;
        for (auto &ps : poly) {
            RemoveDuplicateAndCollinearPoint(ps);
            tmp.emplace_back(&ps);
        }
        dst.emplace_back(tmp);
    }
}

/**
 * @brief 把 PolygonDataI 类型的曼哈顿多边形集合转换成 PolygonPtrDataI 类型的多边形集合， 比PolygonToPtr少了去共线处理
 *
 * @param src 入参，PolygonDataI 类型的多边形集合
 * @param dst 出参，PolygonPtrDataI 类型的多边形集合
 */
inline void ManhattanPolygonToPtr(std::vector<PolygonDataI> &src, std::vector<PolygonPtrDataI> &dst)
{
    dst.clear();
    for (auto &poly : src) {
        PolygonPtrDataI tmp;
        for (auto &ps : poly) {
            tmp.emplace_back(&ps);
        }
        dst.emplace_back(tmp);
    }
}

/**
 * @brief 删除首尾重复的点
 *
 * @param poly 入参/出参， 需要删除首尾重复点序的多边形点集集合
 */
template <class PointType>
inline void RemoveFrontBackRepeatPoly(std::vector<std::vector<PointType>> &poly)
{
    for (auto &ring : poly) {
        if (ring.front() == ring.back()) {
            ring.pop_back();
        }
    }
}

/**
 * @brief 删除首尾重复的点
 *
 * @param polys 入参/出参， 需要删除首尾重复点序的多边形点集集合
 */
template <class PointType>
inline void RemoveFrontBackRepeatPoints(std::vector<std::vector<std::vector<PointType>>> &polys)
{
    for (auto &poly : polys) {
        RemoveFrontBackRepeatPoly(poly);
    }
}

}  // namespace medb2

#endif  // MEDB_POINT_UTILS_H
