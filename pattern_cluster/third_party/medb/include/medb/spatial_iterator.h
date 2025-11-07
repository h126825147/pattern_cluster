/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_SPATIAL_ITERATOR_H
#define MEDB_SPATIAL_ITERATOR_H

#include <algorithm>
#include <vector>

#include "medb/element.h"
#include "medb/spatial_index.h"

namespace medb2 {
// 空间索引迭代器
class SpatialIterator {
public:
    /**
     *  @brief 空间索引迭代器构造函数
     *
     *  @param spatial_index 空间索引对象
     *  @param region 指定的查询区域
     *  @param mode   指定的查询模式
     *  @param query_type   指定的查图形类型种类
     */
    SpatialIterator(const SpatialIndex &spatial_index, const BoxI &region, SpatialQueryMode mode,
                    QueryElementType query_type)
        : spatial_index_(spatial_index), region_(region), mode_(mode), query_type_(query_type)
    {
    }

    SpatialIterator(const SpatialIterator &) = delete;
    SpatialIterator(SpatialIterator &&) = delete;

    SpatialIterator &operator=(const SpatialIterator &) = delete;
    SpatialIterator &operator=(SpatialIterator &&) = delete;

    /**
     *  @brief 空间检索迭代器的查询初始化
     */
    void Begin() { BeginGrid(); }

    /**
     *  @brief 将迭代器指向下一个元素
     */
    void Next()
    {
        if (IsEnd()) {
            return;
        }
        NextGrid();
    }

    /**
     *  @brief 判断迭代器是否迭代完
     *
     * @return true 迭代完
     * @return false 未迭代完
     */
    bool IsEnd() const { return IsEndGrid(); }

    /**
     *  @brief 获取当前迭代器指向的元素
     *
     * @return 当前迭代器指向的元素
     */
    const Element GetObj() const { return current_element_; }

private:
    bool BeginGrid()
    {
        if (spatial_index_.grid_world_box_.Empty() || !IsIntersect(region_, spatial_index_.grid_world_box_)) {
            return false;
        }

        is_region_contain_grid_ = IsContain(region_, spatial_index_.grid_world_box_);

        spatial_index_.GridRange(region_, mode_, grid_range_);
        elements_record_.clear();

        current_grid_index_ = static_cast<uint32_t>(grid_range_.Bottom()) * spatial_index_.columns_ +
                              static_cast<uint32_t>(grid_range_.Left());
        is_current_grid_inside_ = IsCurrentGridInsideRegion();
        if (BeginElement()) {
            return true;
        }
        return NextGrid();
    }

    bool NextGrid()
    {
        if (NextElement()) {
            return true;
        }

        // 更新节点信息
        IncreaseGridIndex();
        while (!IsEndGrid()) {
            is_current_grid_inside_ = IsCurrentGridInsideRegion();
            if (BeginElement()) {
                return true;
            }
            IncreaseGridIndex();
        }
        return false;
    }

    bool IsEndGrid() const
    {
        if (current_grid_index_ == std::numeric_limits<uint32_t>::max()) {
            return true;
        }
        return current_grid_index_ > static_cast<uint32_t>(grid_range_.Top()) * spatial_index_.columns_ +
                                         static_cast<uint32_t>(grid_range_.Right());
    }

    bool IsCurrentGridInsideRegion()
    {
        if (IsEndGrid()) {
            return false;
        }

        if (is_region_contain_grid_) {
            return true;
        }

        // 查询区域
        int32_t query_region_xmin = std::max(region_.Left(), spatial_index_.grid_world_box_.Left());
        int32_t query_region_ymin = std::max(region_.Bottom(), spatial_index_.grid_world_box_.Bottom());
        int32_t query_region_xmax = std::min(region_.Right(), spatial_index_.grid_world_box_.Right());
        int32_t query_region_ymax = std::min(region_.Top(), spatial_index_.grid_world_box_.Top());

        uint32_t current_column = current_grid_index_ % spatial_index_.columns_;
        uint32_t current_row = current_grid_index_ / spatial_index_.columns_;
        int32_t gridpos_xmin = Accumulate(spatial_index_.gridpos_xmin_, current_column * spatial_index_.grid_width_);
        int32_t gridpos_ymin = Accumulate(spatial_index_.gridpos_ymin_, current_row * spatial_index_.grid_height_);
        int32_t gridpos_xmax = Accumulate(gridpos_xmin, spatial_index_.grid_width_);
        int32_t gridpos_ymax = Accumulate(gridpos_ymin, spatial_index_.grid_height_);

        return query_region_xmin <= gridpos_xmin && query_region_ymin <= gridpos_ymin &&
               query_region_xmax >= gridpos_xmax && query_region_ymax >= gridpos_ymax;
    }

    bool BeginElement()
    {
        current_shape_index_ = 0;
        if (query_type_ == QueryElementType::kOnlyInstance) {
            current_shape_index_ = CurrentShapes().size();
        }

        current_instance_index_ = 0;
        if (query_type_ == QueryElementType::kOnlyShape) {
            current_instance_index_ = CurrentInstances().size();
        }

        if (IsCurrentElementValid()) {
            return true;
        }
        return NextElement();
    }

    bool NextElement()
    {
        MoveToNextElement();
        while (!IsEndElement()) {
            if (IsCurrentElementValid()) {
                return true;
            }
            MoveToNextElement();
        }
        return false;
    }

    bool IsEndElement() const
    {
        return (current_shape_index_ >= CurrentShapes().size() && current_instance_index_ >= CurrentInstances().size());
    }

    std::pair<Element, bool> CurrentElement()  // 第二个返回bool值表明是shape还是instance, true是shape
    {
        if (current_shape_index_ < CurrentShapes().size()) {
            return {CurrentShapes()[current_shape_index_], true};
        }

        if (current_instance_index_ < CurrentInstances().size()) {
            return {CurrentInstances()[current_instance_index_], false};
        }

        return {{}, false};
    }

    void MoveToNextElement()
    {
        if (current_shape_index_ < CurrentShapes().size()) {
            current_shape_index_++;
        } else if (current_instance_index_ < CurrentInstances().size()) {
            current_instance_index_++;
        }
    }

    // MODE, 全包含半包含，REGION, duplicate
    bool IsCurrentElementValid()
    {
        auto [element, is_shape] = CurrentElement();
        if (element.Empty()) {
            return false;
        }

        const auto &triple_range = spatial_index_.grid_[current_grid_index_].GetTripleRange(is_shape);
        size_t current_index = is_shape ? current_shape_index_ : current_instance_index_;
        bool is_intersect_region = true;
        do {
            if (mode_ == SpatialQueryMode::kSimple) {  // Simple下格子里所有element都要返回
                break;
            }

            if (is_current_grid_inside_) {
                break;
            }

            if (triple_range.IsGridInsideElement(current_index)) {
                break;
            }

            // 最后需要判断Intersect
            if (spatial_index_.IsIntersect(element, region_)) {
                break;
            }
            is_intersect_region = false;
        } while (false);

        if (!is_intersect_region) {
            return false;
        }

        if (!triple_range.IsElementInsideGrid(current_index)) {
            auto ret = elements_record_.insert(element);
            if (!ret.second) {
                return false;
            }
        }

        current_element_ = element;
        return true;
    }

    void IncreaseGridIndex()
    {
        uint32_t current_column = current_grid_index_ % spatial_index_.columns_;
        if (current_column == static_cast<uint32_t>(grid_range_.Right())) {  // 走到最右换行
            uint32_t current_row = current_grid_index_ / spatial_index_.columns_;
            if (current_row == static_cast<uint32_t>(grid_range_.Top())) {
                current_grid_index_ = std::numeric_limits<uint32_t>::max();
                return;
            }
            current_grid_index_ +=
                spatial_index_.columns_ - Distance(grid_range_.Right(), grid_range_.Left());  // 下一行然后回退
        } else {
            current_grid_index_++;
        }
    }

    const MeVector<Element> &CurrentShapes() const
    {
        return spatial_index_.grid_[current_grid_index_].GetTripleRange(true).Elements();
    }

    const MeVector<Element> &CurrentInstances() const
    {
        return spatial_index_.grid_[current_grid_index_].GetTripleRange(false).Elements();
    }

private:
    const SpatialIndex &spatial_index_;
    BoxI region_;  // query_region
    SpatialQueryMode mode_{SpatialQueryMode::kAccurate};
    QueryElementType query_type_{QueryElementType::kShapeAndInstance};
    MeUnorderedSet<Element> elements_record_;  // 增量存储检索结果，UnorderedSet容器用于去重图形结果集
    BoxI grid_range_;  // 使用boxI来表示网格的范围，并不是真正的box，bottom left代表的行列的最小值，top
                       // right代表行行列的最大值
    uint32_t current_grid_index_{
        std::numeric_limits<uint32_t>::max()};  // 当前网格位置迭代器，初始值为 -1 代表未开始迭代
    bool is_region_contain_grid_{false};        // 当前全部网格是否被query_region全包
    bool is_current_grid_inside_{false};        // 当前网格是否被query_region全包
    Element current_element_;
    size_t current_shape_index_{0};     // 当前element位置迭代器
    size_t current_instance_index_{0};  //
};
}  // namespace medb2
#endif  // MEDB_SPATIAL_ITERATOR_H
