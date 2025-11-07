/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_BOX_H
#define MEDB_BOX_H

#include <string>
#include "medb/allocator.h"
#include "medb/geometry_data.h"
#include "medb/point.h"
#include "medb/transformation.h"

namespace medb2 {

/**
 * @brief 描述一个2D空间的矩形
 *
 * @tparam CoordType 坐标类型，参考 Point
 */
template <class CoordType>
class Box {
public:
    /**
     * @brief Box构造函数
     *
     */
    Box() : bottom_left_(0, 0), top_right_(0, 0) {}

    ~Box() = default;

    /**
     * @brief Box带参构造函数
     *
     * @param x1 Box左下点x坐标
     * @param y1 Box左下点y坐标
     * @param x2 Box右上点x坐标
     * @param y2 Box右上点y坐标
     */
    Box(CoordType x1, CoordType y1, CoordType x2, CoordType y2)
        : bottom_left_(x1 < x2 ? x1 : x2, y1 < y2 ? y1 : y2), top_right_(x2 > x1 ? x2 : x1, y2 > y1 ? y2 : y1)
    {
    }

    /**
     * @brief Box带参构造函数
     *
     * @param p1 Box坐下点
     * @param p2 Box右上点
     */
    Box(const Point<CoordType> &p1, const Point<CoordType> &p2)
        : bottom_left_(p1.X() < p2.X() ? p1.X() : p2.X(), p1.Y() < p2.Y() ? p1.Y() : p2.Y()),
          top_right_(p2.X() > p1.X() ? p2.X() : p1.X(), p2.Y() > p1.Y() ? p2.Y() : p1.Y())
    {
    }

    Box(const Box<CoordType> &) = default;
    Box(Box<CoordType> &&) = default;

    Box<CoordType> &operator=(const Box<CoordType> &) = default;
    Box<CoordType> &operator=(Box<CoordType> &&) = default;

    /**
     * @brief 设置Box的两个点
     *
     * @param p1 被设置Box的左下点
     * @param p2 被设置Box的右上点
     */
    void Set(const Point<CoordType> &p1, const Point<CoordType> &p2)
    {
        bottom_left_.SetX((p1.X() < p2.X()) ? p1.X() : p2.X());
        bottom_left_.SetY((p1.Y() < p2.Y()) ? p1.Y() : p2.Y());
        top_right_.SetX((p2.X() > p1.X()) ? p2.X() : p1.X());
        top_right_.SetY((p2.Y() > p1.Y()) ? p2.Y() : p1.Y());
    }

    /**
     * @brief Box的==运算符重载 判断两个Box是否相等
     *
     * @param b 与之判断像等的Box对象引用
     * @return 相等返回true, 不相等返回false
     */
    bool operator==(const Box<CoordType> &b) const
    {
        return (bottom_left_ == b.BottomLeft() && top_right_ == b.TopRight());
    }

    /**
     * @brief Box的!=运算符重载 判断两个Box是否不等
     *
     * @param b 与之判断不等的Box对象引用
     * @return 不等返回true, 相等返回false
     */
    bool operator!=(const Box<CoordType> &b) const { return !(*this == b); }

    /**
     * @brief Box的<运算符重载 判断一个Box是否小于另一个Box
     *
     * @param b 与之判断小于的Box对象引用
     * @return 小于返回true, 大于返回false
     */
    bool operator<(const Box<CoordType> &b) const
    {
        return bottom_left_ < b.BottomLeft() || (BottomLeft() == b.BottomLeft() && TopRight() < b.TopRight());
    }

    /**
     * @brief 获取Box的左下坐标点
     *
     * @return const Point<CoordType>& Box左下坐标点对象引用
     */
    const Point<CoordType> &BottomLeft() const { return bottom_left_; }

    /**
     * @brief 获取Box的右上坐标点
     *
     * @return const Point<CoordType>& Box右上坐标点对象引用
     */
    const Point<CoordType> &TopRight() const { return top_right_; }

    /**
     * @brief 获取Box的左下坐标点的横坐标
     *
     * @return CoordType Box的左下坐标点的横坐标的值
     */
    CoordType Left() const { return bottom_left_.X(); }

    /**
     * @brief 获取Box的左下坐标点的纵坐标
     *
     * @return CoordType Box的左下坐标点的纵坐标的值
     */
    CoordType Bottom() const { return bottom_left_.Y(); }

    /**
     * @brief 获取Box的右上坐标点的横坐标
     *
     * @return CoordType Box的右上坐标点的横坐标的值
     */
    CoordType Right() const { return top_right_.X(); }

    /**
     * @brief 获取Box的右上坐标点的纵坐标
     *
     * @return CoordType Box的右上坐标点的纵坐标的值
     */
    CoordType Top() const { return top_right_.Y(); }

    /**
     * @brief 获取 box 的包围盒
     *
     */
    const Box<CoordType> &BoundingBox() const { return *this; }

    /**
     * @brief 对 Box 的四个点做变换，然后将变换后的四个点的 Bonding Box 作为结果存入当前对象
     *
     * @tparam TransType 参数的类型
     * @param t 包含变换信息的Transformation对象引用
     * @return Box<CoordType>& 自身的引用
     */
    template <class TransType>
    Box<CoordType> &Transform(const TransType &t)
    {
        Set(t.Transformed(BottomLeft()), t.Transformed(TopRight()));
        return *this;
    }

    /**
     * @brief Box不改变自身做变换
     *
     * @tparam TransType 参数的类型
     * @param t 包含变换信息的Transformation对象引用
     * @return 当前对象变换后的图形的 bonding box
     */
    template <class TransType>
    Box<CoordType> Transformed(const TransType &t) const
    {
        return {t.Transformed(bottom_left_), t.Transformed(top_right_)};
    }

    /**
     * @brief 判断 Box 是否为空
     *
     * @return true 空对象
     * @return false 非空对象
     */
    bool Empty() const { return bottom_left_.X() >= top_right_.X() || bottom_left_.Y() >= top_right_.Y(); }

    /**
     * @brief 清空当前对象
     *
     */
    void Clear()
    {
        bottom_left_.SetX(0);
        bottom_left_.SetY(0);
        top_right_.SetX(0);
        top_right_.SetY(0);
    }

    /**
     * @brief 获取 Box 对象的宽度
     *
     * @return auto Box 对象的宽度
     */

    auto Width() const { return Distance(top_right_.X(), bottom_left_.X()); }

    /**
     * @brief 获取 Box 对象的高度
     *
     * @return auto Box 对象的高度
     */
    auto Height() const { return Distance(top_right_.Y(), bottom_left_.Y()); }

    /**
     * @brief 计算 Box 的面积
     *
     * @return double box 的面积
     */
    double Area() const { return static_cast<double>(Width()) * Height(); }

    /**
     * @brief Box信息转换成string
     *
     * @return std::string Box内部点数据的字符串描述
     */
    std::string ToString() const
    {
        std::string str;
        str.append("(").append(bottom_left_.ToString()).append(",").append(top_right_.ToString()).append(")");
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    Point<CoordType> bottom_left_;
    Point<CoordType> top_right_;
};

using BoxI = Box<int32_t>;
using BoxD = Box<double>;

}  // namespace medb2
#endif  // MEDB_BOX_H
