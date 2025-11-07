/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_SHAPE_REPETITION_H
#define MEDB_SHAPE_REPETITION_H

#include "medb/box.h"
#include "medb/point.h"
#include "medb/point_utils.h"
#include "medb/polygon.h"
#include "medb/repetition.h"
#include "medb/transformation.h"
#include "medb/transformation_utils.h"

namespace medb2 {
/**
 * @brief 用于存储一系列重复的图形（任意两个图形之间都能通过平移得到），支持 BoxI 和 PolygonI
 *
 */
template <class ShapeType>
class MEDB_API ShapeRepetition {
public:
    using InnerShapeType = ShapeType;
    static constexpr bool IsSupportedRepetition()
    {
        return std::is_same_v<ShapeType, BoxI> || std::is_same_v<ShapeType, PolygonI>;
    }

    ShapeRepetition() { static_assert(IsSupportedRepetition(), "Just support BoxI or PolygonI"); };
    /**
     * @brief ShapeRepetition 的构造器
     *
     * @param shape 为 ShapeRepetition 中存储的基础图形，所有其他图形都是通过这个基础图形平移而来
     * @param rep 中存储了所有平移向量
     */
    ShapeRepetition(const ShapeType &shape, const Repetition &rep) : shape_(shape), rep_(rep)
    {
        static_assert(IsSupportedRepetition(), "Just support BoxI or PolygonI");
    }

    ShapeRepetition(const ShapeRepetition &other) = default;
    ShapeRepetition(ShapeRepetition &&other) = default;

    ShapeRepetition &operator=(const ShapeRepetition &other) = default;
    ShapeRepetition &operator=(ShapeRepetition &&other) = default;

    /**
     * @brief 对当前的 ShapeRepetition 进行变换操作
     *
     * @param trans 需要做的变换,包含1.镜像；2.缩放；3.旋转；4.位移
     * @return 改变当前 ShapeRepetition 数据,返回自身
     */
    template <class TransType>
    ShapeRepetition &Transform(const TransType &trans)
    {
        shape_.Transform(trans);
        rep_.TransformWithoutTranslation(trans);
        return *this;
    }

    /**
     * @brief 对当前 ShapeRepetition 进行复制,对副本进行变换操作
     *
     * @param trans 需要做的变换,包含1.镜像；2.缩放；3.旋转；4.位移
     * @return 改变副本 ShapeRepetition 数据,返回副本变换后的 ShapeRepetition
     */
    template <class TransType>
    ShapeRepetition Transformed(const TransType &trans) const
    {
        ShapeRepetition other = *this;
        other.Transform(trans);
        return other;
    }

    /**
     * @brief 获取 ShapeRepetition 的包围盒
     */
    BoxI BoundingBox() const
    {
        if (Size() == 0) {
            return BoxI();
        }
        BoxI offset_box = rep_.BoundingBox();
        BoxI bbox = shape_.BoundingBox();
        bbox.Set(bbox.BottomLeft() + offset_box.BottomLeft(), bbox.TopRight() + offset_box.TopRight());
        return bbox;
    }

    /**
     * @brief 获取 ShapeRepetition 中第 index 个图形的包围盒
     */
    BoxI BoundingBox(size_t index) const
    {
        if (index >= Size()) {
            return BoxI();
        }
        VectorI offset = rep_.Offset(index);
        BoxI box = shape_.BoundingBox();
        box.Set(box.BottomLeft() + offset, box.TopRight() + offset);
        return box;
    }

    /**
     * @brief 获取 ShapeRepetition 中的所有图形
     *
     * @param out 以追加模式将当前 ShapeRepetition 展开后的所有图形添加到 out 后面
     */
    void GetAllShape(MeVector<ShapeType> &out) const
    {
        auto &offsets = rep_.Offsets();
        std::visit(
            [&out, this](auto &&info) {
                using T = std::decay_t<decltype(info)>;
                if (out.empty()) {
                    out.reserve(info.Size());
                }
                if constexpr (std::is_same_v<T, ArrayInfo>) {
                    SimpleTransformation trans;
                    auto total_offset_col = info.OffsetCol() * info.Cols();
                    for (int i = 0; i < static_cast<int>(info.Rows()); ++i) {
                        for (int j = 0; j < static_cast<int>(info.Cols()); ++j) {
                            out.emplace_back(shape_.Transformed(trans));
                            trans.Translation() += info.OffsetCol();
                        }
                        trans.Translation() -= total_offset_col;
                        trans.Translation() += info.OffsetRow();
                    }
                } else {
                    for (int i = 0; i < static_cast<int>(info.Size()); ++i) {
                        out.emplace_back(shape_.Transformed(SimpleTransformation(info.Offset(i))));
                    }
                }
            },
            offsets);
    }

    /**
     * @brief 获取 ShapeRepetition 中的第 index 个图形
     */
    ShapeType GetShape(size_t index) const
    {
        if (index >= Size()) {
            return ShapeType();
        }
        return shape_.Transformed(SimpleTransformation(rep_.Offset(index)));
    }

    /**
     * @brief 获取 ShapeRepetition 中从 start_index 开始的第一个与 region 相交的图形的索引
     *
     * @param region 查询区域
     * @param start_index 从 ShapeRepetition 中该位置的图形开始遍历
     * @return 若查找到与 region 相交的图形，则返回它的索引；否则返回 ShapeRepetition 中图形的个数
     */
    size_t FindIntersected(const BoxI &region, size_t start_index) const
    {
        size_t size = Size();
        while (start_index < size) {
            if (IsIntersect(region, BoundingBox(start_index))) {
                return start_index;
            }
            ++start_index;
        }
        return size;
    }

    /**
     * @brief 检测是否有图形与指定区域相交
     *
     * @param region 待匹配是否相交的区域
     * @return 若存在与指定区域相交的图形，则返回 true，否则返回 false
     */
    bool HasShapeIntersect(const BoxI &region) const
    {
        const BoxI &bbox = RawShape().BoundingBox();
        const BoxI offset_region(region.BottomLeft() - bbox.TopRight(), region.TopRight() - bbox.BottomLeft());
        return RawRepetition().HasOffsetIn(offset_region);
    }

    /**
     * @brief 获取 ShapeRepetition 中的 shape_, 类型为 ShapeType
     */
    const ShapeType &RawShape() const { return shape_; }

    /**
     * @brief 获取 ShapeRepetition 中的 Repetition
     */
    const auto &RawRepetition() const { return rep_; }

    /**
     * @brief 获取 ShapeRepetition 中的 Repetition
     */
    auto &RawRepetition() { return rep_; }

    /**
     * @brief 设置 ShapeRepetition 中的 shape, 类型为 ShapeType
     */
    void SetShape(const ShapeType &shape) { shape_ = shape; }

    /**
     * @brief 设置 ShapeRepetition 中的 Repetition
     */
    void SetRepetition(const Repetition &rep) { rep_ = rep; }

    /**
     * @brief 获取 ShapeRepetition 中的图形的个数
     */
    size_t Size() const { return rep_.Size(); }

    /**
     * @brief 对 Repetition 进行排序，提升 Repetition 中 RegionQuery 的效率
     */
    void Sort() { rep_.Sort(); }

    std::string ToString() const
    {
        std::string str;
        str.append(rep_.ToString()).append("shape: ").append(shape_.ToString()).append("\n");
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    ShapeType shape_{};
    Repetition rep_;
};
}  // namespace medb2

#endif  // MEDB_SHAPE_REPETITION_H