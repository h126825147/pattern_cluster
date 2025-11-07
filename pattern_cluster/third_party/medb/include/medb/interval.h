/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_INTERVAL_H
#define MEDB_INTERVAL_H

#include "medb/base_utils.h"
namespace medb2 {

template <class CoordType>
class Range {
public:
    // 默认构造函数
    Range() = default;
    ~Range() = default;

    /**
     * @brief 区间构造函数，设置上下限阈值及其开闭区间
     *
     * @param lower 区间下限
     * @param upper 区间上限
     */
    Range(CoordType lower, CoordType upper) : lower_(lower), upper_(upper) {}

    /**
     * @brief 设置区间下限
     *
     * @param lower 区间下限
     */
    void SetLower(CoordType lower) { lower_ = lower; }

    /**
     * @brief 设置区间上限
     *
     * @param upper 区间上限
     */
    void SetUpper(CoordType upper) { upper_ = upper; }

    /**
     * @brief 判断当前区间与另一个区间是否相交，考虑接触的情况
     */
    bool Intersect(const Range &other) const
    {
        return CoordLessEqual(lower_, other.upper_) && CoordGreaterEqual(upper_, other.lower_);
    }

    /**
     * @brief 判断当前区间与另一个区间是否相交，不考虑接触的情况
     */
    bool IntersectNotTouch(const Range &other) const
    {
        return CoordLess(lower_, other.upper_) && CoordGreater(upper_, other.lower_);
    }

    /**
     * @brief 判断两个区间是否相等
     */
    bool operator==(const Range &other) const
    {
        return CoordEqual(lower_, other.lower_) && CoordEqual(upper_, other.upper_);
    }

    /**
     * @brief 判断两个区间是否不等
     */
    bool operator!=(const Range &other) const { return !(*this == other); }

    /**
     * @brief 获取区间下界
     */
    CoordType Lower() const { return lower_; }

    /**
     * @brief 获取区间上界
     */
    CoordType Upper() const { return upper_; }

private:
    CoordType lower_{0};
    CoordType upper_{0};
};

template <class CoordType>
class Interval {
public:
    Interval() = default;
    ~Interval() = default;
    /**
     * @brief 阈值构造函数，设置上下限阈值及其开闭区间
     *
     * @param lower             阈值下限
     * @param upper             阈值上限
     * @param lower_close       阈值下限开闭开关：true：闭区间，false：开区间
     * @param upper_close       阈值上限开闭开关：true：闭区间，false：开区间
     */
    Interval(CoordType lower, CoordType upper, bool lower_close, bool upper_close)
        : range_(lower, upper), lower_close_(lower_close), upper_close_(upper_close)
    {
    }

    /**
     * @brief 判断阈值内是否有实数值
     *
     * @return true: 存在有效值，false：不存在有效值
     */
    bool IsVaild() const
    {
        return CoordLess(Lower(), Upper()) || (CoordEqual(Lower(), Upper()) && lower_close_ && upper_close_);
    }

    /**
     * @brief 判断 x 是否在阈值范围内
     *
     * @return true: 在范围内，false：不在范围内
     */
    bool Contain(CoordType x) const
    {
        return (lower_close_ ? CoordLessEqual(Lower(), x) : CoordLess(Lower(), x)) &&
               (upper_close_ ? CoordLessEqual(x, Upper()) : CoordLess(x, Upper()));
    }

    /**
     * @brief 获取阈值下限值
     *
     * @return 阈值下限值
     */
    CoordType Lower() const { return range_.Lower(); }

    /**
     * @brief 获取阈值上限值
     *
     * @return 阈值上限值
     */
    CoordType Upper() const { return range_.Upper(); }
    /**
     * @brief 获取阈值下限值开闭开关
     *
     * @return 阈值阈值下限值开闭开关
     */
    bool LowerClose() const { return lower_close_; }
    /**
     * @brief 获取阈值上限值开闭开关
     *
     * @return 阈值阈值上限值开闭开关
     */
    bool UpperClose() const { return upper_close_; }

private:
    Range<CoordType> range_;
    bool lower_close_{false};
    bool upper_close_{false};
};

}  // namespace medb2
#endif