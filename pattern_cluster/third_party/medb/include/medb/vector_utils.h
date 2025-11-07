/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_VECTOR_OPERATION_H
#define MEDB_VECTOR_OPERATION_H

#include "medb/base_utils.h"
#include "medb/enums.h"
#include "medb/hash.h"
#include "medb/point.h"

namespace medb2 {
template <class CoordType>
auto CrossProduct(const Vector<CoordType> &vec_a, const Vector<CoordType> &vec_b)
{
    if constexpr (std::is_integral_v<CoordType>) {
        return static_cast<int64_t>(vec_a.X()) * static_cast<int64_t>(vec_b.Y()) -
               static_cast<int64_t>(vec_b.X()) * static_cast<int64_t>(vec_a.Y());
    } else {
        return vec_a.X() * vec_b.Y() - vec_b.X() * vec_a.Y();
    }
}

template <class CoordType>
auto DotProduct(const Vector<CoordType> &vec_a, const Vector<CoordType> &vec_b)
{
    if constexpr (std::is_integral_v<CoordType>) {
        return static_cast<int64_t>(vec_a.X()) * static_cast<int64_t>(vec_b.X()) +
               static_cast<int64_t>(vec_a.Y()) * static_cast<int64_t>(vec_b.Y());
    } else {
        return vec_a.X() * vec_b.X() + vec_a.Y() * vec_b.Y();
    }
}

template <class CoordTypeA, class CoordTypeB>
inline Vector<CoordTypeA> operator*(const Vector<CoordTypeA> &vec, CoordTypeB factor)
{
    Vector<CoordTypeA> vec_ret(CoordCvt<CoordTypeA>(vec.X() * factor), CoordCvt<CoordTypeA>(vec.Y() * factor));
    return vec_ret;
}

template <class CoordTypeA, class CoordTypeB>
inline Vector<CoordTypeA> &operator*=(Vector<CoordTypeA> &vec, CoordTypeB factor)
{
    vec.SetX(CoordCvt<CoordTypeA>(vec.X() * factor));
    vec.SetY(CoordCvt<CoordTypeA>(vec.Y() * factor));
    return vec;
}

template <class CoordTypeA, class CoordTypeB>
inline Vector<CoordTypeA> operator/(const Vector<CoordTypeA> &vec, CoordTypeB factor)
{
    Vector<CoordTypeA> vec_ret(CoordCvt<CoordTypeA>(vec.X() / factor), CoordCvt<CoordTypeA>(vec.Y() / factor));
    return vec_ret;
}

template <class CoordTypeA, class CoordTypeB>
inline Vector<CoordTypeA> &operator/=(Vector<CoordTypeA> &vec, CoordTypeB factor)
{
    vec.SetX(CoordCvt<CoordTypeA>(vec.X() / factor));
    vec.SetY(CoordCvt<CoordTypeA>(vec.Y() / factor));
    return vec;
}

template <class CoordTypeA, class CoordTypeB>
inline Vector<CoordTypeA> operator+(const Vector<CoordTypeA> &vec_a, const Vector<CoordTypeB> &vec_b)
{
    Point<CoordTypeA> pt_ret(CoordCvt<CoordTypeA>(vec_a.X() + vec_b.X()), CoordCvt<CoordTypeA>(vec_a.Y() + vec_b.Y()));
    return pt_ret;
}

template <class CoordTypeA, class CoordTypeB>
inline Vector<CoordTypeA> &operator+=(Vector<CoordTypeA> &vec_a, const Vector<CoordTypeB> &vec_b)
{
    vec_a.SetX(CoordCvt<CoordTypeA>(vec_a.X() + vec_b.X()));
    vec_a.SetY(CoordCvt<CoordTypeA>(vec_a.Y() + vec_b.Y()));
    return vec_a;
}

template <class CoordType>
inline auto SafeAdd(const Vector<CoordType> &vec_a, const Vector<CoordType> &vec_b)
{
    using TraitsCoordType = typename CoordTraits<CoordType>::OverFlowType;
    Vector<TraitsCoordType> pt_ret((static_cast<TraitsCoordType>(vec_a.X()) + static_cast<TraitsCoordType>(vec_b.X())),
                                   (static_cast<TraitsCoordType>(vec_a.Y()) + static_cast<TraitsCoordType>(vec_b.Y())));
    return pt_ret;
}

template <class CoordTypeA, class CoordTypeB>
inline Vector<CoordTypeA> operator-(const Vector<CoordTypeA> &vec_a, const Vector<CoordTypeB> &vec_b)
{
    Point<CoordTypeA> pt_ret(CoordCvt<CoordTypeA>(vec_a.X() - vec_b.X()), CoordCvt<CoordTypeA>(vec_a.Y() - vec_b.Y()));
    return pt_ret;
}

template <class CoordTypeA, class CoordTypeB>
inline Vector<CoordTypeA> &operator-=(Vector<CoordTypeA> &vec_a, const Vector<CoordTypeB> &vec_b)
{
    vec_a.SetX(CoordCvt<CoordTypeA>(vec_a.X() - vec_b.X()));
    vec_a.SetY(CoordCvt<CoordTypeA>(vec_a.Y() - vec_b.Y()));
    return vec_a;
}

template <class CoordType>
inline auto SafeSub(const Vector<CoordType> &vec_a, const Vector<CoordType> &vec_b)
{
    using TraitsCoordType = typename CoordTraits<CoordType>::OverFlowType;
    Vector<TraitsCoordType> pt_ret((static_cast<TraitsCoordType>(vec_a.X()) - static_cast<TraitsCoordType>(vec_b.X())),
                                   (static_cast<TraitsCoordType>(vec_a.Y()) - static_cast<TraitsCoordType>(vec_b.Y())));
    return pt_ret;
}

template <class CoordType>
inline Vector<CoordType> operator-(const Vector<CoordType> &vec)
{
    Point<CoordType> pt_ret(-vec.X(), -vec.Y());
    return pt_ret;
}

template <class CoordType>
double VectorLength(const Vector<CoordType> &v)
{
    double dx(static_cast<double>(v.X()));
    double dy(static_cast<double>(v.Y()));
    return sqrt(dx * dx + dy * dy);
}

// 获取单位向量
template <class CoordType>
inline Vector<CoordType> Unit(const Vector<CoordType> &v)
{
    double len = VectorLength(v);
    if (fabs(len) < kDoubleEPS) {
        return v;
    }
    Point<CoordType> pt_ret(v.X() / len, v.Y() / len);
    return pt_ret;
}

// 获取 90 度夹角的法向量
template <class CoordType>
inline Vector<CoordType> Normal90(const Vector<CoordType> &v)
{
    Vector<CoordType> pt_ret(-v.Y(), v.X());
    return pt_ret;
}

// 获取 90 度夹角的单位法向量
template <class CoordType>
inline Vector<CoordType> UnitNormal90(const Vector<CoordType> &v)
{
    return Unit(Normal90(v));
}

/**
 * @brief 对向量进行以90度为倍数的逆时针旋转，修改原始的
 * vector。注意，由于整型变量的最小值比最大值的范围更大，这里不对含最小值的 Vector 做变换
 *
 * @param vector 入参，出参，需要进行旋转的点
 * @param rotation 入参，需要旋转的角度
 * @return Vector<CoordType>& 返回 vector 的引用
 */
template <class CoordType>
inline Vector<CoordType> &RotatePoint(Vector<CoordType> &vector, const RotationType &rotation)
{
    if constexpr (std::is_integral<CoordType>::value) {
        if (vector.X() == std::numeric_limits<CoordType>::lowest() ||
            vector.Y() == std::numeric_limits<CoordType>::lowest()) {
            return vector;
        }
    }
    auto temp = vector;
    switch (rotation) {
        case kRotation90:
            vector.SetX(-temp.Y());
            vector.SetY(temp.X());
            break;
        case kRotation180:
            vector.SetX(-temp.X());
            vector.SetY(-temp.Y());
            break;
        case kRotation270:
            vector.SetX(temp.Y());
            vector.SetY(-temp.X());
            break;
        default:  // 0 or 360
            break;
    }
    return vector;
}

/**
 * @brief 对向量进行以90度为倍数的逆时针旋转，不修改原始的 vector
 *
 * @param vector 入参，需要进行旋转的点
 * @param rotation 入参，需要旋转的角度
 * @return Vector<CoordType> 返回旋转后的向量
 */
template <class CoordType>
inline Vector<CoordType> RotatedPoint(const Vector<CoordType> &vector, const RotationType &rotation)
{
    Vector<CoordType> other_v(vector);
    return RotatePoint(other_v, rotation);
}

}  // namespace medb2

#endif
