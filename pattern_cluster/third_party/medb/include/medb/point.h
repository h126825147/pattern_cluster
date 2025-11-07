/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_POINT_H
#define MEDB_POINT_H

#include <cmath>
#include <string>
#include <type_traits>
#include <vector>
#include "medb/allocator.h"
#include "medb/base_utils.h"

namespace medb2 {
template <class CoordType>
class Point;

template <class CoordType>
using Vector = Point<CoordType>;
using VectorI = Vector<int32_t>;
using VectorD = Vector<double>;

/**
 * @brief Point表示一个点，包含坐标x_和y_。
 */
template <class CoordType>
class Point {
public:
    Point() = default;
    ~Point() = default;
    /**
     * @brief 根据给出的x坐标和y坐标构造一个Point对象。
     * @param x 需要赋值给Point对象的x坐标。
     * @param y 需要赋值给Point对象的y坐标。
     */
    Point(CoordType x, CoordType y) : x_(x), y_(y) {}

    Point(const Point<CoordType> &) = default;
    Point(Point<CoordType> &&) = default;

    /**
     * @brief 主要用来将原来的Point对象Point<D>转换成新的Point对象Point<CoordType>。
     * @param p 用来新Point对象的点p。
     */
    template <class D>
    explicit Point(const Point<D> &p) : x_(CoordCvt<CoordType, D>(p.X())), y_(CoordCvt<CoordType, D>(p.Y()))
    {
    }

    Point<CoordType> &operator=(const Point<CoordType> &) = default;
    Point<CoordType> &operator=(Point<CoordType> &&) = default;

    /**
     * @brief 修改Point对象的x坐标
     * @param x 坐标x。
     */
    void SetX(CoordType x) { x_ = x; }

    /**
     * @brief 修改Point对象的y坐标
     * @param y 坐标y。
     */
    void SetY(CoordType y) { y_ = y; }

    /**
     * @brief 判断p和当前Point对象是否相等。
     * @param p 需要和当前Point对象做比较操作的点。
     * @return 相等返回true，否则返回false。
     */
    bool operator==(const Point<CoordType> &p) const { return CoordEqual(x_, p.x_) && CoordEqual(y_, p.y_); }

    /**
     * @brief 判断p和当前Point对象是否不等。
     * @param p 需要和当前Point对象做比较操作的点。
     * @return 不等返回true，否则返回false。
     */
    bool operator!=(const Point<CoordType> &p) const { return !(*this == p); }

    /**
     * @brief 判断p是否小于当前Point对象。其实就是按y值比较。
     * @param p 需要和当前Point对象做比较操作的点。
     * @return 小于返回true，否则返回false。
     */
    bool operator<(const Point<CoordType> &p) const
    {
        return CoordLess(x_, p.x_) || (CoordEqual(x_, p.x_) && CoordLess(y_, p.y_));
    }

    /**
     * @brief 获取坐标x值。
     * @return 坐标x。
     */
    CoordType X() const { return x_; }

    /**
     * @brief 获取坐标y值。
     * @return 坐标y。
     */
    CoordType Y() const { return y_; }

    /**
     * @brief 将Point转换成字符串。
     * @return 返回转换后的字符串。
     */
    std::string ToString() const
    {
        std::string str;
        str += "{";
        str += std::to_string(X());
        str += ",";
        str += std::to_string(Y());
        str += "}";
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    CoordType x_{0};
    CoordType y_{0};
};

using PointI = Point<int32_t>;
using PointD = Point<double>;

// 描述一个闭环曲线的数据
template <class CoordType>
using RingData = std::vector<Point<CoordType>>;
using RingDataI = std::vector<PointI>;
template <class CoordType>
using MeRingData = MeVector<Point<CoordType>>;
using MeRingDataI = MeVector<PointI>;

// 用多个闭环曲线描述一个多边形的数据
template <class CoordType>
using PolygonPtrData = std::vector<RingData<CoordType> *>;
using PolygonPtrDataI = std::vector<RingDataI *>;
template <class CoordType>
using MePolygonPtrData = MeVector<MeRingData<CoordType> *>;
using MePolygonPtrDataI = MeVector<MeRingDataI *>;

template <class CoordType>
using PolygonData = std::vector<RingData<CoordType>>;
using PolygonDataI = std::vector<RingDataI>;
template <class CoordType>
using MePolygonData = MeVector<MeRingData<CoordType>>;
using MePolygonDataI = MeVector<MeRingDataI>;

}  // namespace medb2
#endif  // MEDB_POINT_H
