/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef BOX_UTILS_H
#define BOX_UTILS_H

#include "box.h"
#include "hash.h"
#include "point_utils.h"

namespace std {
template <class CoordType>
class hash<medb2::Box<CoordType> > {
public:
    // 将四个不同的坐标值的哈希码混合在一起，生成唯一的哈希码，用于表示一个 medb2::Box 对象的哈希值
    size_t operator()(const medb2::Box<CoordType> &box) const
    {
        size_t seed = 0;
        medb2::HashCombine(seed, box.Bottom());
        medb2::HashCombine(seed, box.Left());
        medb2::HashCombine(seed, box.Top());
        medb2::HashCombine(seed, box.Right());
        return seed;
    }
};
}  // namespace std

namespace medb2 {
template <>
class PtrHash<BoxI> {
public:
    size_t operator()(const BoxI *ptr) const { return std::hash<BoxI>()(*ptr); }
};

template <>
class PtrEqual<BoxI> {
public:
    size_t operator()(const BoxI *ptr1, const BoxI *ptr2) const { return *ptr1 == *ptr2; }
};

template <class CoordType>
void BoxUnion(Box<CoordType> &box_target, const Box<CoordType> &box_other)
{
    if (box_other.BottomLeft() == box_other.TopRight()) {
        return;
    }
    if (box_target.BottomLeft() == box_target.TopRight()) {
        box_target = box_other;
        return;
    }
    CoordType left = std::min(box_target.BottomLeft().X(), box_other.BottomLeft().X());
    CoordType bottom = std::min(box_target.BottomLeft().Y(), box_other.BottomLeft().Y());
    CoordType right = std::max(box_target.TopRight().X(), box_other.TopRight().X());
    CoordType top = std::max(box_target.TopRight().Y(), box_other.TopRight().Y());
    box_target = Box<CoordType>(left, bottom, right, top);
}

// 判断 2 个区域是否是 intersect 关系
template <class CoordType>
bool IsIntersect(const Box<CoordType> &region1, const Box<CoordType> &region2)
{
    auto region1_min_x = region1.BottomLeft().X();
    auto region1_min_y = region1.BottomLeft().Y();
    auto region1_max_x = region1.TopRight().X();
    auto region1_max_y = region1.TopRight().Y();

    auto region2_min_x = region2.BottomLeft().X();
    auto region2_min_y = region2.BottomLeft().Y();
    auto region2_max_x = region2.TopRight().X();
    auto region2_max_y = region2.TopRight().Y();

    return region1_max_x >= region2_min_x && region1_min_x <= region2_max_x && region1_max_y >= region2_min_y &&
           region1_min_y <= region2_max_y;
}

template <class CoordType>
bool IsIntersectNotTouch(const Box<CoordType> &region1, const Box<CoordType> &region2)
{
    auto region1_min_x = region1.BottomLeft().X();
    auto region1_min_y = region1.BottomLeft().Y();
    auto region1_max_x = region1.TopRight().X();
    auto region1_max_y = region1.TopRight().Y();

    auto region2_min_x = region2.BottomLeft().X();
    auto region2_min_y = region2.BottomLeft().Y();
    auto region2_max_x = region2.TopRight().X();
    auto region2_max_y = region2.TopRight().Y();

    return region1_max_x > region2_min_x && region1_min_x < region2_max_x && region1_max_y > region2_min_y &&
           region1_min_y < region2_max_y;
}

// 判断 b2是否完全在b1里面的关系，包含接触的情况
template <class CoordType>
bool IsContain(const Box<CoordType> &b1, const Box<CoordType> &b2)
{
    if ((b2.BottomLeft().X() >= b1.BottomLeft().X() && b2.TopRight().X() <= b1.TopRight().X()) &&
        (b2.BottomLeft().Y() >= b1.BottomLeft().Y() && b2.TopRight().Y() <= b1.TopRight().Y())) {
        return true;
    } else {
        return false;
    }
}

// 判断 b2是否完全在b1里面的关系
template <class CoordType>
bool IsContainNotTouch(const Box<CoordType> &b1, const Box<CoordType> &b2)
{
    if ((b2.BottomLeft().X() > b1.BottomLeft().X() && b2.TopRight().X() < b1.TopRight().X()) &&
        (b2.BottomLeft().Y() > b1.BottomLeft().Y() && b2.TopRight().Y() < b1.TopRight().Y())) {
        return true;
    } else {
        return false;
    }
}

// 判断 point 是否在 box 里面
template <class CoordType>
bool IsContain(const Box<CoordType> &box, const Point<CoordType> &point)
{
    return point.X() >= box.Left() && point.X() <= box.Right() && point.Y() >= box.Bottom() && point.Y() <= box.Top();
}

// 判断 point 是否在 box 里面，不包含边界
template <class CoordType>
bool IsContainNotTouch(const Box<CoordType> &box, const Point<CoordType> &point)
{
    return point.X() > box.Left() && point.X() < box.Right() && point.Y() > box.Bottom() && point.Y() < box.Top();
}

}  // namespace medb2

#endif  // BOX_UTILS_H