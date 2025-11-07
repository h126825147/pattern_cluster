
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_BASE_UTILS_H
#define MEDB_BASE_UTILS_H
#include <cmath>
#include <type_traits>
#include "medb/const.h"

#ifdef _WIN32
// MSVC版本
#define MEDB_LIKELY(x) (x)
#define MEDB_UNLIKELY(x) (x)
#define MEDB_API __declspec(dllexport)
#define MEDB_HIDDEN  // MSVC 无直接等效属性，默认不导出即可
#else
// GCC/Clang版本
#define MEDB_LIKELY(x) __builtin_expect(!!(x), 1)
#define MEDB_UNLIKELY(x) __builtin_expect(!!(x), 0)
#define MEDB_API
#define MEDB_HIDDEN __attribute__((visibility("hidden")))
#endif

namespace medb2 {
inline bool FloatLess(float x, float y) { return x - y < -kFloatEPS; }
inline bool FloatGreater(float x, float y) { return x - y > kFloatEPS; }
inline bool FloatEqual(float x, float y) { return fabs(x - y) <= kFloatEPS; }
inline bool DoubleEqual(double x, double y) { return fabs(x - y) <= kDoubleEPS; }
inline bool DoubleLess(double x, double y) { return x - y < -kDoubleEPS; }
inline bool DoubleGreater(double x, double y) { return x - y > kDoubleEPS; }
inline bool DoubleLessEqual(double x, double y) { return !DoubleGreater(x, y); }
inline bool DoubleGreaterEqual(double x, double y) { return !DoubleLess(x, y); }

template <class CoordType>
inline bool CoordLess(CoordType c0, CoordType c1)
{
    if constexpr (std::is_same<CoordType, float>::value) {
        return FloatLess(c0, c1);
    } else if constexpr (std::is_same<CoordType, double>::value) {
        return DoubleLess(c0, c1);
    } else {
        return c0 < c1;
    }
}

template <class CoordType>
inline bool CoordGreater(CoordType c0, CoordType c1)
{
    if constexpr (std::is_same<CoordType, float>::value) {
        return FloatGreater(c0, c1);
    } else if constexpr (std::is_same<CoordType, double>::value) {
        return DoubleGreater(c0, c1);
    } else {
        return c0 > c1;
    }
}

template <class CoordType>
inline bool CoordLessEqual(CoordType c0, CoordType c1)
{
    return !CoordGreater(c0, c1);
}

template <class CoordType>
inline bool CoordGreaterEqual(CoordType c0, CoordType c1)
{
    return !CoordLess(c0, c1);
}

template <class CoordType>
inline bool CoordEqual(CoordType c0, CoordType c1)
{
    if constexpr (std::is_same<CoordType, float>::value) {
        return FloatEqual(c0, c1);
    } else if constexpr (std::is_same<CoordType, double>::value) {
        return DoubleEqual(c0, c1);
    } else {
        return c0 == c1;
    }
}

// 坐标类型转换，从浮点转整型时，若是正数，正常四舍五入；若是负数，则是大于五则入，否则舍；此处理的目的是使得求相交点时在正负数上的舍入偏移是一致的。
template <class CoordTypeA, class CoordTypeB>
inline CoordTypeA CoordCvt(CoordTypeB x)
{
    if constexpr (std::is_integral<CoordTypeA>::value && !std::is_integral<CoordTypeB>::value) {
        CoordTypeA int_part = static_cast<CoordTypeA>(x);
        CoordTypeB float_part = x - static_cast<CoordTypeB>(int_part);
        // 大部分场景下，float_part 等于 0
        if (MEDB_UNLIKELY(float_part < -0.5 - kDoubleEPS)) {
            return int_part - 1;
        } else if (MEDB_LIKELY(float_part < 0.5 - kDoubleEPS)) {
            return int_part;
        }
        return int_part + 1;
    } else {
        return static_cast<CoordTypeA>(x);
    }
}

inline double DegreeToRadian(double degree) { return degree / kDegrees180 * kPI; }

// 计算数值 b 到数值 a 的距离，请确保 a > b
template <class CoordType>
inline auto Distance(CoordType a, CoordType b)
{
    if constexpr (std::is_floating_point_v<CoordType>) {
        return static_cast<double>(a) - static_cast<double>(b);
    } else {
        // 必须强转成unsigned再计算，因为signed减法溢出是未定义行为，而unsigned减法溢出和强转unsigned是确定行为，一定会回绕。
        return std::make_unsigned_t<CoordType>(a) - std::make_unsigned_t<CoordType>(b);
    }
}

// 计算一个有符号和一个无符号的和，保证所有值都在范围内，比如INT_MIN+UINT_MAX=INT_MAX
template <class CoordType1, class CoordType2>
inline auto Accumulate(CoordType1 begin, CoordType2 length)
{
    static_assert(std::is_signed_v<CoordType1> && std::is_unsigned_v<CoordType2> &&
                  sizeof(CoordType1) == sizeof(CoordType2));
    // 必须强转成unsigned再计算，因为signed减法溢出是未定义行为，而unsigned减法溢出和强转unsigned是确定行为，一定会回绕。
    // 最后结果要转回sign。
    return static_cast<CoordType1>(static_cast<CoordType2>(begin) + length);
}

template <class T>
class CoordTraits {};

template <>
class CoordTraits<int32_t> {
public:
    using OverFlowType = int64_t;
};

template <>
class CoordTraits<double> {
public:
    using OverFlowType = double;
};

}  // namespace medb2

#endif  // MEDB_BASE_UTILS_H
