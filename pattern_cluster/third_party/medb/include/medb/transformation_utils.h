/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_TRANSFORMATION_UTILS_H
#define MEDB_TRANSFORMATION_UTILS_H

#include <variant>
#include "medb/transformation.h"

namespace medb2 {
/**
 * @brief 组合两个 Transformation 对象。
 * 考虑这样的场景：Cell 层级顺序：A(root) -> B(sub1) -> C(sub2) ; c 为 C 中的坐标点，计算 c 在 A 上的坐标值。
 * 假设 cb 为 C 在 B 上的 Trans 信息， ba 为 B 在 A 上的 Trans 信息。
 * 则一般的计算方法为：
 *    1.算出 c 在 C 上的坐标(即 c)；
 *    2.算出 c 在 B 上的坐标即 cb.Transform(c)；
 *    3.通过步骤2中得到的结果算出 c 在 A 上的坐标 ba.Transform(cb.Transform(c));
 * 以上步骤中，执行了 2 次 transform, 且这些步骤顺序不可变，即意味着这个计算过程不满足交换律。
 * 有了 Compose 接口后，可以减少 transform 的次数，一般步骤如下：
 *    1.算出 c 在 C 上的坐标(即 c)；
 *    2.用 ba 去组合 cb, 即 Compose(ba, cb)；这里要注意不要将顺序搞反了；
 *    3.通过步骤 2 中组合的结果算出 c 在 A 上的坐标, 即 Compose(ba, cb).Transform(c);
 * 这样看，似乎没必要去组合，那是因为本例中只有 1 个点，而版图中实际场景中，跟多的是有多个坐标点具有相同的多级
 * Trans 信息，此时通过 Compose 接口能减少大量计算。另外，对于 int 型坐标，多次 Transform 会导致误差的累积，使
 * 用组合(Compose)后的 Transform 能极大减少这种误差。
 *
 * @param trans_a 要组合的第一个 Transformation 对象
 * @param trans_b 要组合的第二个 Transformation 对象
 * @return Transformation 组合后的本对象
 */
inline Transformation Compose(const Transformation &trans_a, const Transformation &trans_b)
{
    auto translation = trans_a.Transformed(trans_b.Translation());
    RotationType rotation;
    if (trans_a.Magnification() < 0.0) {
        rotation = static_cast<RotationType>((trans_a.Rotation() - trans_b.Rotation() + kRotation360) % kRotation360);
    } else {
        rotation = static_cast<RotationType>((trans_a.Rotation() + trans_b.Rotation()) % kRotation360);
    }
    double magnification = trans_a.Magnification() * trans_b.Magnification();
    return Transformation(translation, rotation, magnification);
}

inline Transformation Compose(const Transformation &trans_a, const SimpleTransformation &trans_b)
{
    auto translation = trans_a.Transformed(Vector<double>(trans_b.Translation()));
    auto rotation = trans_a.Rotation();
    double magnification = trans_a.Magnification();
    return Transformation(translation, rotation, magnification);
}

inline Transformation Compose(const SimpleTransformation &trans_a, const Transformation &trans_b)
{
    auto translation = trans_a.Transformed(trans_b.Translation());
    auto rotation = trans_b.Rotation();
    double magnification = trans_b.Magnification();
    return Transformation(translation, rotation, magnification);
}

inline SimpleTransformation Compose(const SimpleTransformation &trans_a, const SimpleTransformation &trans_b)
{
    auto translation = trans_a.Transformed(trans_b.Translation());
    return SimpleTransformation(translation);
}

template <class TransType>
inline TransformationVar Compose(const TransformationVar &trans_a, const TransType &trans_b)
{
    return std::visit([&trans_b](auto &&trans) -> TransformationVar { return Compose(trans, trans_b); }, trans_a);
}

template <class TransType>
inline TransformationVar Compose(const TransType &trans_a, const TransformationVar &trans_b)
{
    return std::visit([&trans_a](auto &&trans) -> TransformationVar { return Compose(trans_a, trans); }, trans_b);
}

inline TransformationVar Compose(const TransformationVar &trans_a, const TransformationVar &trans_b)
{
    return std::visit([](auto &&trans_a_inner,
                         auto &&trans_b_inner) -> TransformationVar { return Compose(trans_a_inner, trans_b_inner); },
                      trans_a, trans_b);
}

}  // namespace medb2

#endif  // MEDB_TRANSFORMATION_UTILS_H