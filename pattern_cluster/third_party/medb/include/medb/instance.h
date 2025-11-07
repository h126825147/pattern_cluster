/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_INSTANCE_H
#define MEDB_INSTANCE_H

#include "medb/allocator.h"
#include "medb/box.h"
#include "medb/placement.h"

namespace medb2 {

class Cell;
class Layer;
/**
 * @brief 映射于版图文件中的 Cell & Placement 的概念
 * Placement 指针中 trans 的类型有 SimpleTransformation 和 Transformation 两种；repetition 可能有也可能没有。
 */
class MEDB_API Instance {
public:
    Instance() = default;

    ~Instance() = default;

    /**
     * @brief 带参构造函数
     *
     * @param c 被引用的 Cell
     * @param p Placement 信息
     */
    Instance(Cell *c, Placement::UniquePtrType p) : cell_ptr_(c), placement_ptr_(std::move(p)) {}

    Instance(const Instance &other)
        : cell_ptr_(other.cell_ptr_), placement_ptr_(Placement::CopyPlacement(other.placement_ptr_.get()))
    {
    }

    Instance(Instance &&other) noexcept : cell_ptr_(other.cell_ptr_), placement_ptr_(std::move(other.placement_ptr_))
    {
        other.cell_ptr_ = nullptr;
    }

    Instance &operator=(const Instance &other)
    {
        if (this != &other) {
            cell_ptr_ = other.cell_ptr_;
            placement_ptr_ = Placement::CopyPlacement(other.placement_ptr_.get());
        }
        return *this;
    }

    Instance &operator=(Instance &&other) noexcept
    {
        if (this != &other) {
            cell_ptr_ = other.cell_ptr_;
            other.cell_ptr_ = nullptr;
            placement_ptr_ = std::move(other.placement_ptr_);
        }
        return *this;
    }

    /**
     * @brief 获取被引用的 Cell 对象指针
     *
     * @return Cell* 被引用的 Cell 对象指针
     */
    Cell *CellPtr() const { return cell_ptr_; }

    /**
     * @brief 设置被引用的 Cell 对象指针
     *
     * @param cell 被引用的 Cell 对象指针
     */
    void SetCellPtr(Cell *cell) { cell_ptr_ = cell; }

    /**
     * @brief 获取被引用的 Placement 对象指针
     *
     * @return Placement* 被引用的 Placement 对象指针
     */
    const Placement *PlacementPtr() const { return placement_ptr_.get(); }

    /**
     * @brief 设置被引用的 Placement 对象指针
     *
     * @param placement 被引用的 Placement 对象指针
     */
    void SetPlacementPtr(Placement::UniquePtrType placement) { placement_ptr_ = std::move(placement); }

    /**
     * @brief 对 placement 进行 Transform 变换, 可能会改变 placement 的类型
     *
     * @param trans Transformation 或 SimpleTransformation 对象
     * @return bool 表示设置是否成功
     */
    template <class TransType>
    bool TransformPlacement(const TransType &trans)
    {
        if (!IsValid()) {
            return false;
        }
        // 优先尝试不改变 placement 类型的 Transform
        if (placement_ptr_->Transform(trans)) {
            return true;
        }
        // 必须要改变 placement 类型时，调用 Placement 的 Transformed 函数
        placement_ptr_ = std::move(placement_ptr_->Transformed(trans));
        return true;
    }

    /**
     * @brief 判断当前对象有效，cell 与 placement 同时有效才返回 true
     *
     * @return true 是
     * @return false 不是
     */
    bool IsValid() const
    {
        return cell_ptr_ != nullptr && placement_ptr_ && placement_ptr_->Type() != kInvalidPlacement;
    }

    /**
     * @brief 获取Instance的指定layer的Bounding box
     *
     * @return BoxI 是 Instance的单个layer上的图形的包围盒（Bounding box）
     */
    BoxI GetBoundingBox(const Layer &layer) const;

    /**
     * @brief 获取Instance的所有layer的Bounding box
     *
     * @return BoxI 是 Instance的所有layer上所有图形的包围盒（Bounding box）
     */
    BoxI GetBoundingBox() const;

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    Cell *cell_ptr_{nullptr};
    Placement::UniquePtrType placement_ptr_{nullptr};
};

}  // namespace medb2

#endif  // MEDB_INSTANCE_H
