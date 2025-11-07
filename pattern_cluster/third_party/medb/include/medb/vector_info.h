/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_VECTOR_INFO_H
#define MEDB_VECTOR_INFO_H

#include <variant>
#include "medb/allocator.h"
#include "medb/box.h"
#include "medb/box_utils.h"
#include "medb/point.h"
#include "medb/point_utils.h"

namespace medb2 {

class VerticalVectorInfo;

/**
 * @brief 描述一系列在 x 轴上的偏移向量，是 Repetition 的一种特殊类型。
 * x_coords_ 需满足以下两个条件才能正常写入 oasis 文件中：
 * 1. x_coords_ 的第 0 个元素为 0
 * 2. x_coords_ 有序
 */
class HorizontalVectorInfo {
public:
    explicit HorizontalVectorInfo(const MeVector<int32_t> &x_coords) : x_coords_(x_coords) {}

    const MeVector<int32_t> &XCoords() const { return x_coords_; }

    void SetXCoords(const MeVector<int32_t> &x_coords) { x_coords_ = x_coords; }

    /**
     * @brief 获取 HorizontalVectorInfo 中所有偏移向量指向的点的包围盒
     */
    BoxI BoundingBox() const
    {
        return BoxI(Offset(0), Offset(Size() - 1));  // HorizontalVectorInfo 内部需保证有序
    }

    /**
     * @brief 对 HorizontalVectorInfo 进行 Transform 操作，包含旋转、镜像、缩放，不包含位移
     *
     * @return std::variant 由于进行 Transform 操作后，VectorInfo 的类型可能产生变化，因此这里返回 variant
     */
    template <class TransType>
    std::variant<HorizontalVectorInfo, VerticalVectorInfo> TransformedWithoutTranslation(const TransType &trans);

    /**
     * @brief 获取 HorizontalVectorInfo 中的第 i 个偏移向量
     */
    VectorI Offset(size_t i) const
    {
        if (i >= Size()) {
            return {0, 0};
        }
        return VectorI(x_coords_[i], 0);
    }

    size_t Size() const { return x_coords_.size(); }

    /**
     * @brief 获取所有在 region 中的偏移向量
     *  若偏移向量 offset 满足以下条件，则将其输出
     *  region_min_x <= offset_x <= region_max_x
     *  region_min_y <= offset_y <= region_max_y
     *
     * @return MeVector<VectorI> 返回所有在 region 中的偏移向量
     */
    MeVector<VectorI> RegionQuery(const BoxI &region) const
    {
        MeVector<VectorI> res;
        if (region.Top() < 0 || region.Bottom() > 0) {
            return res;
        }
        bool ascending = x_coords_[0] <= x_coords_[x_coords_.size() - 1];
        if (ascending) {
            auto begin = std::lower_bound(x_coords_.begin(), x_coords_.end(), region.Left());
            auto end = std::upper_bound(x_coords_.begin(), x_coords_.end(), region.Right());
            while (begin < end) {
                res.emplace_back(*begin, 0);
                ++begin;
            }
        } else {
            auto begin = std::lower_bound(x_coords_.rbegin(), x_coords_.rend(), region.Left());
            auto end = std::upper_bound(x_coords_.rbegin(), x_coords_.rend(), region.Right());
            while (begin < end) {
                res.emplace_back(*begin, 0);
                ++begin;
            }
        }
        return res;
    }

    /**
     * @brief 检查当前对象中的偏移向量是否有落在 region 中的
     *
     * @return bool true 表示有偏移向量落在 region 中
     */
    bool HasOffsetIn(const BoxI &region) const
    {
        if (region.Top() < 0 || region.Bottom() > 0) {
            return false;
        }
        bool ascending = x_coords_[0] <= x_coords_[x_coords_.size() - 1];
        if (ascending) {
            auto begin = std::lower_bound(x_coords_.begin(), x_coords_.end(), region.Left());
            auto end = std::upper_bound(x_coords_.begin(), x_coords_.end(), region.Right());
            return begin < end;
        } else {
            auto begin = std::lower_bound(x_coords_.rbegin(), x_coords_.rend(), region.Left());
            auto end = std::upper_bound(x_coords_.rbegin(), x_coords_.rend(), region.Right());
            return begin < end;
        }
    }

    std::string ToString() const
    {
        std::string str = "VectorType: Horizontal\noffsets: ";
        std::for_each(x_coords_.begin(), x_coords_.end(),
                      [&str](int32_t offset) { str.append("{" + std::to_string(offset) + ",0}, "); });
        str.append("\n");
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    MeVector<int32_t> x_coords_;
};

/**
 * @brief 描述一系列在 y 轴上的偏移向量，是 Repetition 的一种特殊类型。
 * y_coords_ 需满足以下两个条件才能正常写入 oasis 文件中：
 * 1. y_coords_ 的第 0 个元素为 0
 * 2. y_coords_ 有序
 */
class VerticalVectorInfo {
public:
    explicit VerticalVectorInfo(const MeVector<int32_t> &y_coords) : y_coords_(y_coords) {}

    const MeVector<int32_t> &YCoords() const { return y_coords_; }

    void SetYCoords(const MeVector<int32_t> &y_coords) { y_coords_ = y_coords; }

    /**
     * @brief 获取 VerticalVectorInfo 中所有偏移向量指向的点的包围盒
     */
    BoxI BoundingBox() const
    {
        return BoxI(Offset(0), Offset(Size() - 1));  // VerticalVectorInfo 内部需保证有序
    }

    /**
     * @brief 对 VerticalVectorInfo 进行 Transform 操作，包含旋转、镜像、缩放，不包含位移
     *
     * @return std::variant 由于进行 Transform 操作后，VectorInfo 的类型可能产生变化，因此这里返回 variant
     */
    template <class TransType>
    std::variant<HorizontalVectorInfo, VerticalVectorInfo> TransformedWithoutTranslation(const TransType &trans);

    /**
     * @brief 获取 VerticalVectorInfo 中的第 i 个偏移向量
     */
    VectorI Offset(size_t i) const
    {
        if (i >= Size()) {
            return {0, 0};
        }
        return VectorI(0, y_coords_[i]);
    }

    size_t Size() const { return y_coords_.size(); }

    /**
     * @brief 获取所有在 region 中的偏移向量
     *  若偏移向量 offset 满足以下条件，则将其输出
     *  region_min_x <= offset_x <= region_max_x
     *  region_min_y <= offset_y <= region_max_y
     *
     * @return MeVector<VectorI> 返回所有在 region 中的偏移向量
     */
    MeVector<VectorI> RegionQuery(const BoxI &region) const
    {
        MeVector<VectorI> res;
        if (region.Right() < 0 || region.Left() > 0) {
            return res;
        }
        bool ascending = y_coords_[0] <= y_coords_[y_coords_.size() - 1];
        if (ascending) {
            auto begin = std::lower_bound(y_coords_.begin(), y_coords_.end(), region.Bottom());
            auto end = std::upper_bound(y_coords_.begin(), y_coords_.end(), region.Top());
            while (begin < end) {
                res.emplace_back(0, *begin);
                ++begin;
            }
        } else {
            auto begin = std::lower_bound(y_coords_.rbegin(), y_coords_.rend(), region.Bottom());
            auto end = std::upper_bound(y_coords_.rbegin(), y_coords_.rend(), region.Top());
            while (begin < end) {
                res.emplace_back(0, *begin);
                ++begin;
            }
        }

        return res;
    }

    /**
     * @brief 检查当前对象中的偏移向量是否有落在 region 中的
     *
     * @return bool true 表示有偏移向量落在 region 中
     */
    bool HasOffsetIn(const BoxI &region) const
    {
        if (region.Right() < 0 || region.Left() > 0) {
            return false;
        }
        bool ascending = y_coords_[0] <= y_coords_[y_coords_.size() - 1];
        if (ascending) {
            auto begin = std::lower_bound(y_coords_.begin(), y_coords_.end(), region.Bottom());
            auto end = std::upper_bound(y_coords_.begin(), y_coords_.end(), region.Top());
            return begin < end;
        } else {
            auto begin = std::lower_bound(y_coords_.rbegin(), y_coords_.rend(), region.Bottom());
            auto end = std::upper_bound(y_coords_.rbegin(), y_coords_.rend(), region.Top());
            return begin < end;
        }
    }

    std::string ToString() const
    {
        std::string str = "VectorType: Vertical\noffsets: ";
        std::for_each(y_coords_.begin(), y_coords_.end(),
                      [&str](int32_t offset) { str.append("{0," + std::to_string(offset) + "}, "); });
        str.append("\n");
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    MeVector<int32_t> y_coords_;
};

class OrdinaryVectorInfo {
public:
    explicit OrdinaryVectorInfo(const MeVector<VectorI> &offsets)
    {
        size_ = static_cast<uint32_t>(offsets.size());
        if (size_ == 0) {
            return;
        }
        offsets_ = new VectorI[size_];
        std::copy(offsets.data(), offsets.data() + offsets.size(), offsets_);
    }

    OrdinaryVectorInfo(const OrdinaryVectorInfo &other)
        : size_(other.size_), y_min_(other.y_min_), y_max_(other.y_max_), sorted_(other.sorted_)
    {
        if (offsets_ != nullptr) {
            delete[] offsets_;
            offsets_ = nullptr;
        }
        if (size_ == 0) {
            return;
        }
        offsets_ = new VectorI[size_];
        std::copy(other.offsets_, other.offsets_ + other.size_, offsets_);
    }

    OrdinaryVectorInfo(OrdinaryVectorInfo &&other) noexcept
        : size_(other.size_), y_min_(other.y_min_), y_max_(other.y_max_), sorted_(other.sorted_)
    {
        if (offsets_ != nullptr) {
            delete[] offsets_;
            offsets_ = nullptr;
        }
        offsets_ = other.offsets_;
        other.size_ = 0;
        other.offsets_ = nullptr;
        other.ClearMarks();
    }

    OrdinaryVectorInfo &operator=(const OrdinaryVectorInfo &other)
    {
        if (this == &other) {
            return *this;
        }
        size_ = other.size_;
        if (offsets_ != nullptr) {
            delete[] offsets_;
            offsets_ = nullptr;
        }
        if (size_ > 0) {
            offsets_ = new VectorI[size_];
            std::copy(other.offsets_, other.offsets_ + other.size_, offsets_);
        }
        y_min_ = other.y_min_;
        y_max_ = other.y_max_;
        sorted_ = other.sorted_;
        return *this;
    }

    OrdinaryVectorInfo &operator=(OrdinaryVectorInfo &&other) noexcept
    {
        if (this == &other) {
            return *this;
        }
        size_ = other.size_;
        if (offsets_ != nullptr) {
            delete[] offsets_;
            offsets_ = nullptr;
        }
        offsets_ = other.offsets_;
        y_min_ = other.y_min_;
        y_max_ = other.y_max_;
        sorted_ = other.sorted_;
        other.size_ = 0;
        other.offsets_ = nullptr;
        other.ClearMarks();
        return *this;
    }

    ~OrdinaryVectorInfo()
    {
        size_ = 0;
        if (offsets_ != nullptr) {
            delete[] offsets_;
            offsets_ = nullptr;
        }
        ClearMarks();
    }

    const VectorI *Offsets() const { return offsets_; }

    void SetOffsets(const MeVector<VectorI> &offsets)
    {
        size_ = static_cast<uint32_t>(offsets.size());
        if (offsets_ != nullptr) {
            delete[] offsets_;
            offsets_ = nullptr;
        }
        if (size_ > 0) {
            offsets_ = new VectorI[size_];
            std::copy(offsets.data(), offsets.data() + offsets.size(), offsets_);
        }
        ClearMarks();
    }

    /**
     * @brief 获取 OrdinaryVectorInfo 中所有偏移向量指向的点的包围盒，只有经过排序后的 OrdinaryVectorInfo 才能生成
     * BoundingBox 缓存，线程安全
     */
    BoxI BoundingBox() const
    {
        if (offsets_ == nullptr) {
            return BoxI();
        }
        if (!sorted_) {
            return GetBoundingBox(offsets_, size_);
        }
        return BoxI{offsets_[0].X(), y_min_, offsets_[size_ - 1].X(), y_max_};
    }

    /**
     * @brief 获取 OrdinaryVectorInfo 中的第 i 个偏移向量
     */
    VectorI Offset(size_t i) const
    {
        if (i >= Size()) {
            return {0, 0};
        }
        return offsets_[i];
    }

    size_t Size() const { return size_; }

    /**
     * @brief 对 OrdinaryVectorInfo 进行原地 Transform，包含旋转、镜像、缩放，不包含位移
     *
     * @return const OrdinaryVectorInfo& 返回 OrdinaryVectorInfo 自身对象的引用
     */
    template <class TransType>
    const OrdinaryVectorInfo &TransformWithoutTranslation(const TransType &trans)
    {
        if constexpr (std::is_same_v<TransType, Transformation>) {
            for (int i = 0; i < static_cast<int>(size_); ++i) {
                auto &offset = offsets_[i];
                if (DoubleLess(trans.Magnification(), 0.0)) {
                    offset.SetY(-offset.Y());
                }
                offset *= fabs(trans.Magnification());
                RotatePoint(offset, trans.Rotation());
            }
            ClearMarks();
        }
        return *this;
    }

    /**
     * @brief 对 OrdinaryVectorInfo 的拷贝进行 Transform，包含旋转、镜像、缩放，不包含位移
     *
     * @return OrdinaryVectorInfo 返回拷贝后并进行了 Transform 操作的 OrdinaryVectorInfo
     */
    template <class TransType>
    OrdinaryVectorInfo TransformedWithoutTranslation(const TransType &trans) const
    {
        OrdinaryVectorInfo result = *this;
        result.TransformWithoutTranslation(trans);
        return result;
    }

    /**
     * @brief 对 offsets_ 进行排序，能够提升 RegionQuery 的效率
     */
    void Sort()
    {
        if (offsets_ == nullptr || sorted_) {
            return;
        }
        std::sort(offsets_, offsets_ + size_);
        y_min_ = std::numeric_limits<int32_t>::max();
        y_max_ = std::numeric_limits<int32_t>::min();
        for (int i = 0; i < static_cast<int>(size_); ++i) {
            y_min_ = std::min(y_min_, offsets_[i].Y());
            y_max_ = std::max(y_max_, offsets_[i].Y());
        }
        sorted_ = true;
    }

    /**
     * @brief 获取所有在 region 中的偏移向量
     *  注意：若未排序，则通过遍历的方式查找。可能有多个线程同时调用 RegionQuery，因此不能在该函数中进行排序。
     *
     * @return MeVector<VectorI> 返回所有在 region 中的偏移向量
     */
    MeVector<VectorI> RegionQuery(const BoxI &region) const
    {
        MeVector<VectorI> res;
        if (offsets_ == nullptr) {
            return res;
        }
        if (!sorted_) {
            for (int i = 0; i < static_cast<int>(size_); ++i) {
                auto &vec = offsets_[i];
                if (IsContain(region, vec)) {
                    res.emplace_back(vec);
                }
            }
            return res;
        }
        int32_t start_y = region.Bottom();
        int32_t end_y = region.Top();
        auto begin = std::upper_bound(offsets_, offsets_ + size_, region.Left(),
                                      [](int32_t x_min, const VectorI &vector) { return x_min <= vector.X(); });
        auto end = std::lower_bound(offsets_, offsets_ + size_, region.Right(),
                                    [](const VectorI &vector, int32_t x_max) { return x_max >= vector.X(); });

        while (begin < end) {
            if (begin->Y() >= start_y && begin->Y() <= end_y) {
                res.emplace_back(*begin);
            }
            ++begin;
        }
        return res;
    }

    /**
     * @brief 检查当前对象中的偏移向量是否有落在 region 中的
     *
     * @return bool true 表示有偏移向量落在 region 中
     */
    bool HasOffsetIn(const BoxI &region) const
    {
        if (offsets_ == nullptr) {
            return false;
        }
        if (!sorted_) {
            for (int i = 0; i < static_cast<int>(size_); ++i) {
                const auto &vec = offsets_[i];
                if (IsContain(region, vec)) {
                    return true;
                }
            }
            return false;
        }
        int32_t start_y = region.Bottom();
        int32_t end_y = region.Top();
        auto begin = std::upper_bound(offsets_, offsets_ + size_, region.Left(),
                                      [](int32_t x_min, const VectorI &vector) { return x_min <= vector.X(); });
        auto end = std::lower_bound(offsets_, offsets_ + size_, region.Right(),
                                    [](const VectorI &vector, int32_t x_max) { return x_max >= vector.X(); });

        while (begin < end) {
            if (begin->Y() >= start_y && begin->Y() <= end_y) {
                return true;
            }
            ++begin;
        }
        return false;
    }

    std::string ToString() const
    {
        std::string str = "VectorType: Ordinary\noffsets: ";
        std::for_each(offsets_, offsets_ + size_, [&str](VectorI offset) { str.append(offset.ToString() + ", "); });
        str.append("\n");
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    void ClearMarks()
    {
        y_min_ = 0;
        y_max_ = 0;
        sorted_ = false;
    }

private:
    VectorI *offsets_{nullptr};
    uint32_t size_{0};
    int32_t y_min_{std::numeric_limits<int32_t>::max()};
    int32_t y_max_{std::numeric_limits<int32_t>::min()};
    mutable bool sorted_{false};
};

template <class TransType>
std::variant<HorizontalVectorInfo, VerticalVectorInfo> HorizontalVectorInfo::TransformedWithoutTranslation(
    const TransType &trans)
{
    if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
        return *this;
    }
    using ResultType = std::variant<HorizontalVectorInfo, VerticalVectorInfo>;
    auto coords = x_coords_;
    std::for_each(coords.begin(), coords.end(), [&trans](auto &coord) { coord = trans.Scale(coord); });
    //         ↑ 2
    //         |
    // 3 ——————|——————> 1
    //         |
    //         | 4
    // 旋转 0° 时，坐标的值不变
    // 旋转 90° 时，1->2, 3->4，坐标的值不变
    // 旋转 180° 时，1->3, 3->1, 坐标的值都是原来的值取反
    // 旋转 270° 时，1->4, 3->2，坐标的值是原来的值取反
    if (trans.Rotation() == kRotation180 || trans.Rotation() == kRotation270) {
        std::for_each(coords.begin(), coords.end(), [](auto &coord) { coord = -coord; });
    }
    return (trans.Rotation() % kRotation180 == 0) ? ResultType(HorizontalVectorInfo(coords))
                                                  : ResultType(VerticalVectorInfo(coords));
}

template <class TransType>
std::variant<HorizontalVectorInfo, VerticalVectorInfo> VerticalVectorInfo::TransformedWithoutTranslation(
    const TransType &trans)
{
    if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
        return *this;
    }
    using ResultType = std::variant<HorizontalVectorInfo, VerticalVectorInfo>;
    auto coords = y_coords_;
    if (DoubleLess(trans.Magnification(), 0.0)) {
        std::for_each(coords.begin(), coords.end(), [](auto &coord) { coord = -coord; });
    }
    std::for_each(coords.begin(), coords.end(), [&trans](auto &coord) { coord = trans.Scale(coord); });
    //         ↑ 2
    //         |
    // 3 ——————|——————> 1
    //         |
    //         | 4
    // 旋转 0° 时，坐标的值不变
    // 旋转 90° 时，2->3, 4->1，坐标的值都是原来的值取反
    // 旋转 180° 时，2->4, 4->2, 坐标的值都是原来的值取反
    // 旋转 270° 时，2->1, 4->3，坐标的值不变
    if (trans.Rotation() == kRotation180 || trans.Rotation() == kRotation90) {
        std::for_each(coords.begin(), coords.end(), [](auto &coord) { coord = -coord; });
    }
    return (trans.Rotation() % kRotation180 == 0) ? ResultType(VerticalVectorInfo(coords))
                                                  : ResultType(HorizontalVectorInfo(coords));
}

}  // namespace medb2

#endif  // MEDB_ARRAY_INFO_H