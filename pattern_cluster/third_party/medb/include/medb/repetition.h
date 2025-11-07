/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_REPETITION_H
#define MEDB_REPETITION_H

#include <variant>
#include "medb/allocator.h"
#include "medb/array_info.h"
#include "medb/box.h"
#include "medb/point.h"
#include "medb/point_utils.h"
#include "medb/transformation.h"
#include "medb/vector_info.h"

namespace medb2 {
class Repetition {
public:
    Repetition() = default;
    /**
     * @brief Repetition 的构造器，OffsetType 的类型为 VectorType 或 ArrayType，分别对应两种不同的 Repetition 类型
     *
     * @param offsets 中存储了所有平移向量
     */
    template <class OffsetType>
    explicit Repetition(const OffsetType &offsets) : offsets_(offsets)
    {
    }

    Repetition(const Repetition &other) = default;
    Repetition(Repetition &&other) noexcept = default;

    Repetition &operator=(const Repetition &other) = default;
    Repetition &operator=(Repetition &&other) noexcept = default;

    /**
     * @brief 获取 Repetition 的包围盒
     *
     * @return BoxI 表示所有偏移向量所指向的点构成的图形的包围盒
     */
    BoxI BoundingBox() const
    {
        return std::visit([](auto &&info) -> BoxI { return info.BoundingBox(); }, offsets_);
    }

    /**
     * @brief 检查当前对象中的偏移向量是否有落在 region 中的
     *
     * @return bool true 表示有偏移向量落在 region 中
     */
    bool HasOffsetIn(const BoxI &region) const
    {
        return std::visit([&region](auto &&info) -> bool { return info.HasOffsetIn(region); }, offsets_);
    }

    /**
     * @brief 获取 Repetition 的类型，Repetition 的类型有且仅有 Array 和 Vector 两种
     *
     * @return bool 返回 true 表示当前 Repetition 是 Array 类型的，否则是 Vector 类型的
     */
    bool IsArrayType() const { return std::holds_alternative<ArrayInfo>(offsets_); }

    /**
     * @brief 获取 Repetition 的偏移向量个数
     */
    size_t Size() const
    {
        return std::visit([](auto &&info) -> size_t { return info.Size(); }, offsets_);
    }

    /**
     * @brief 获取 Repetition 中第 index 个偏移向量
     *
     * @return 返回第 index 个偏移向量，若 index 超出范围，则返回 {0, 0}
     */
    VectorI Offset(size_t index) const
    {
        return std::visit([&index](auto &&info) -> VectorI { return info.Offset(index); }, offsets_);
    }

    /**
     * @brief 返回内部 offsets_ 变量
     *
     * @return 返回内部变量 offsets_ 的常量引用
     */
    const auto &Offsets() const { return offsets_; }

    /**
     * @brief 返回内部 offsets_ 变量
     *
     * @return 返回内部变量 offsets_ 的引用
     */
    auto &Offsets() { return offsets_; }

    /**
     * @brief 设置 Repetition 的内部变量，OffsetType 的类型为 VectorType 或 ArrayType，分别对应两种不同的 Repetition
     * 类型
     *
     * @param offsets 中存储了所有平移向量
     */
    template <class OffsetType>
    void SetOffsets(const OffsetType &offsets)
    {
        offsets_ = offsets;
    }

    /**
     * @brief 对 Repetition 进行原地 Transform，包含旋转、镜像、缩放，不包含位移
     *
     * @return const Repetition& 返回 Repetition 自身对象的引用
     */
    template <class TransType>
    const Repetition &TransformWithoutTranslation(const TransType &trans)
    {
        if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
            return *this;
        }
        std::visit(
            [this, &trans](auto &&info) {
                using T = std::decay_t<decltype(info)>;
                if constexpr (std::is_same_v<T, ArrayInfo> || std::is_same_v<T, OrdinaryVectorInfo>) {
                    info.TransformWithoutTranslation(trans);
                } else {
                    std::visit([this](auto &&transformed_info) { this->offsets_ = transformed_info; },
                               info.TransformedWithoutTranslation(trans));
                }
            },
            offsets_);
        return *this;
    }

    /**
     * @brief 对 Repetition 的拷贝进行 Transform，包含旋转、镜像、缩放，不包含位移
     *
     * @return Repetition 返回拷贝后并进行了 Transform 操作的 Repetition
     */
    template <class TransType>
    Repetition TransformedWithoutTranslation(const TransType &trans) const
    {
        Repetition result = *this;
        result.TransformWithoutTranslation(trans);
        return result;
    }

    /**
     * @brief 对 Repetition 进行排序，提升 RegionQuery 的效率
     */
    void Sort()
    {
        // ArrayInfo 不需要排序，HorizontalVectorInfo 和 VerticalVectorInfo 默认是有序的
        if (std::holds_alternative<OrdinaryVectorInfo>(offsets_)) {
            std::get<OrdinaryVectorInfo>(offsets_).Sort();
        }
    }

    MeVector<VectorI> RegionQuery(const BoxI &vector_region) const
    {
        return std::visit([&vector_region](auto &&info) { return info.RegionQuery(vector_region); }, offsets_);
    }

    /**
     * @brief 将 Repetition 转换为字符串，便于 debug
     */
    std::string ToString() const
    {
        std::string str;
        str.append("repetition_type: ");
        str.append(IsArrayType() ? "ArrayRepetition\n" : "VectorRepetition\n");
        str.append(std::visit([](auto &&info) -> std::string { return info.ToString(); }, offsets_));
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    std::variant<ArrayInfo, OrdinaryVectorInfo, HorizontalVectorInfo, VerticalVectorInfo> offsets_;
};

}  // namespace medb2

#endif  // MEDB_REPETITION_H