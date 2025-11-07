/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_EDGE_H
#define MEDB_EDGE_H

#include <cassert>
#include <string>
#include "medb/enums.h"
#include "medb/point.h"

namespace medb2 {

/**
 * @brief Edge表示一条边，包含两个点begin_, end_。有关点的含义见Point类。
 */
template <class CoordType>
class Edge {
public:
    Edge() = default;
    ~Edge() = default;

    /**
     * @brief 根据给出的坐标x1、y1和x2、y2构造新的Edge对象。
     */
    Edge(CoordType x1, CoordType y1, CoordType x2, CoordType y2) : begin_(x1, y1), end_(x2, y2) {}
    /**
     * @brief 根据给出的坐标点p1和p2构造新的Edge对象。
     */
    Edge(const Point<CoordType> &p1, const Point<CoordType> &p2) : begin_(p1), end_(p2) {}

    Edge(const Edge<CoordType> &) = default;
    Edge(Edge<CoordType> &&) = default;

    Edge<CoordType> &operator=(const Edge<CoordType> &) = default;
    Edge<CoordType> &operator=(Edge<CoordType> &&) = default;

    Point<CoordType> &BeginPoint() { return begin_; }
    Point<CoordType> &EndPoint() { return end_; }
    /**
     * @brief 获取Edge的起始点。
     * @return 返回起始点的值。
     */
    const Point<CoordType> &BeginPoint() const { return begin_; }
    /**
     * @brief 获取Edge的结束点。
     * @return 返回结束点的值。
     */
    const Point<CoordType> &EndPoint() const { return end_; }

    /**
     * @brief 获取Edge的角度。
     * @return 返回Edge的角度。
     */
    AngleType Angle() const
    {
        auto deltx = end_.X() - begin_.X();
        auto delty = end_.Y() - begin_.Y();
        if constexpr (std::is_same<CoordType, double>::value) {
            if (DoubleEqual(delty, 0.0)) {
                return DoubleGreater(deltx, 0.0) ? kDegree0 : kDegree180;
            }
            if (DoubleEqual(deltx, 0.0)) {
                return DoubleGreater(delty, 0.0) ? kDegree90 : kDegree270;
            }
        } else {
            if (delty == 0) {
                return deltx > 0 ? kDegree0 : kDegree180;
            }
            if (deltx == 0) {
                return delty > 0 ? kDegree90 : kDegree270;
            }
        }
        return kOtherAngle;
    }

    /**
     * @brief 获取Edge的长度。
     * @return 返回Edge的长度。
     */
    double Length() const
    {
        double length = 0.0;
        switch (this->Angle()) {
            case kDegree0:
            case kDegree180:
                length = abs(begin_.X() - end_.X());
                break;
            case kDegree90:
            case kDegree270:
                length = abs(begin_.Y() - end_.Y());
                break;
            case kOtherAngle:
                length = std::sqrt(std::pow(begin_.X() - end_.X(), 2) + std::pow(begin_.Y() - end_.Y(), 2));
                break;
            default:
                break;
        }
        return length;
    }

    /**
     * @brief 设置Edge的起始点和结束点。
     * @param p1 要设置的起始点。
     * @param p2 要设置的起始点。
     */
    void Set(const Point<CoordType> &p1, const Point<CoordType> &p2)
    {
        begin_ = p1;
        end_ = p2;
    }

    /**
     * @brief 判断边e和当前边是否相等。
     * @param e 需要和当前Edge对象做比较的边。
     * @return 相等返回true，否则返回false。
     */
    bool operator==(const Edge<CoordType> &e) const
    {
        return BeginPoint() == e.BeginPoint() && EndPoint() == e.EndPoint();
    }
    /**
     * @brief 判断边e和当前边是否不等。
     * @param e 需要和当前Edge对象做比较的边。
     * @return 不等返回true，否则返回false。
     */
    bool operator!=(const Edge<CoordType> &e) const { return !(*this == e); }

    /**
     * @brief 判断边e是否小于当前Edge对象。其实就是根据起始点begin来判断。
     * @param e 需要和当前Edge对象做比较的边。
     * @return 小于返回true，否则返回false。
     */
    bool operator<(const Edge<CoordType> &e) const
    {
        return begin_ < e.begin_ || (begin_ == e.begin_ && end_ < e.end_);
    }
    /**
     * @brief 将Edge转换为字符串。
     * @return 返回edge对应的字符串。
     */
    std::string ToString() const
    {
        std::string str;
        str += "{";
        str += begin_.ToString();
        str += ",";
        str += end_.ToString();
        str += "}";
        return str;
    }
    Point<CoordType> begin_;
    Point<CoordType> end_;
};

using EdgeI = Edge<int32_t>;
using EdgeD = Edge<double>;

/**
 * @brief Edge90 表示一条水平边或垂直边，仅支持 int32_t 数值类型。
 * 它包含一个 base 坐标表示水平边的 Y 值或垂直边的 X 值，以及 min 和 max 坐标值表示水平边在 X 轴上或垂直边在 Y
 * 轴上的最小值和最大值。
 */
template <uint8_t type>
class Edge90 {
public:
    Edge90() = default;

    /**
     * @brief 根据给定的两个点构造 Edge90，要求这两个点构成的线段水平或垂直
     */
    Edge90(const PointI &p1, const PointI &p2)
    {
        if constexpr (type == EToI(OrientationType::kHorizontal)) {
            assert(p1.Y() == p2.Y());
            base_ = p1.Y();
            min_ = std::min(p1.X(), p2.X());
            max_ = std::max(p1.X(), p2.X());
        } else {  // OrientationType::kVertical
            assert(p1.X() == p2.X());
            base_ = p1.X();
            min_ = std::min(p1.Y(), p2.Y());
            max_ = std::max(p1.Y(), p2.Y());
        }
    }

    /**
     * @brief 根据给定 base，min 和 max 构造 Edge90
     */
    Edge90(int32_t base, int32_t min, int32_t max) : base_(base), min_(min), max_(max) {}

    /**
     * @brief 判断当前线段是否为空
     */
    bool Empty() const { return min_ >= max_; }

    /**
     * @brief 获取 base 值
     */
    int32_t Base() const { return base_; }

    /**
     * @brief 获取 min 值
     */
    int32_t Min() const { return min_; }

    /**
     * @brief 获取 max 值
     */
    int32_t Max() const { return max_; }

    /**
     * @brief 将 Edge90 转换为 EdgeI
     */
    EdgeI ToEdge() const
    {
        if constexpr (type == EToI(OrientationType::kHorizontal)) {
            return {min_, base_, max_, base_};
        } else {
            return {base_, min_, base_, max_};
        }
    }

    /**
     * @brief 设置 base 值
     */
    void Set(int32_t base, int32_t min, int32_t max)
    {
        base_ = base;
        min_ = min;
        max_ = max;
    }

    /**
     * @brief 设置 base 值
     */
    void SetBase(int32_t base) { base_ = base; }

    /**
     * @brief 设置 min 值
     */
    void SetMin(int32_t min) { min_ = min; }

    /**
     * @brief 设置 max 值
     */
    void SetMax(int32_t max) { max_ = max; }

    /**
     * @brief 比较两个水平边或两个垂直边的大小
     */
    bool operator<(const Edge90<type> &other) const
    {
        if (base_ != other.base_) {
            return base_ < other.base_;
        }
        if (min_ != other.min_) {
            return min_ < other.min_;
        }
        return max_ < other.max_;
    }

private:
    int32_t base_{0};
    int32_t min_{0};
    int32_t max_{0};
};

using VerticalEdge = Edge90<EToI(OrientationType::kVertical)>;
using HorizontalEdge = Edge90<EToI(OrientationType::kHorizontal)>;

}  // namespace medb2
#endif  // MEDB_EDGE_H
