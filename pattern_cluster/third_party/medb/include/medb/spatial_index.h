/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_SPATIAL_INDEX_H
#define MEDB_SPATIAL_INDEX_H

#include <algorithm>
#include <cmath>
#include "medb/base_utils.h"
#include "medb/box.h"
#include "medb/box_utils.h"
#include "medb/element.h"
#include "medb/element_utils.h"
#include "medb/geometry_utils.h"
#include "medb/instance.h"
#include "medb/polygon.h"

namespace medb2 {
class SpatialIterator;
class MEDB_API SpatialIndexOption {
public:
    SpatialIndexOption() = default;
    explicit SpatialIndexOption(const std::optional<Layer> &layer) : instance_layer_(layer) {}
    SpatialIndexOption(uint32_t window_step, BoxI region, const std::optional<Layer> &layer = std::nullopt)
        : window_step_(window_step), region_(region), instance_layer_(layer)  // ->region
    {
    }

    static constexpr uint32_t kNodeCapacity = 2000;
    static constexpr uint32_t kRowMin = 1;
    static constexpr uint32_t kRowMax = 100;
    uint32_t WindowStep() const { return window_step_; }
    void SetRegion(const BoxI &box) { region_ = box; }
    const BoxI &Region() const { return region_; }
    void SetInstanceLayer(const std::optional<Layer> &layer) { instance_layer_ = layer; }
    const std::optional<Layer> &InstanceLayer() const { return instance_layer_; }

private:
    uint32_t window_step_{
        0};        // 为 0 则按 kNodeCapacity 初始化索引网格行数和列数，不为 0 则按 window_step_ 初始化索引网格
    BoxI region_;  // 网格区域,若为空，则从 shapes 容器计算实际的 bbox
    std::optional<Layer> instance_layer_{std::nullopt};
};

class GridNode {
public:
    // 存储element数据，分为三段，第一段是grid inside element的，第二段是intersect，第三段是element inside grid的
    class TripleRange {
    public:
        TripleRange() = default;
        ~TripleRange() = default;
        void InsertElementInside(const Element &element) { elements_.emplace_back(element); }

        void InsertGridInside(const Element &element)
        {  // grid inside element在第一段，需要第二、三段后移
            elements_.emplace_back();
            if (element_inside_offset_ != elements_.size()) {  // 第三段为非空
                elements_.back() = elements_[element_inside_offset_];
            }
            if (intersect_offset_ != element_inside_offset_) {  // 第二段为非空
                elements_[element_inside_offset_] = elements_[intersect_offset_];
            }
            elements_[intersect_offset_] = element;

            element_inside_offset_++;
            intersect_offset_++;
        }

        void InsertIntersect(const Element &element)
        {  // intersect在第二段，需要第三段后移
            elements_.emplace_back();
            if (element_inside_offset_ != elements_.size()) {  // 第三段为非空
                elements_.back() = elements_[element_inside_offset_];
            }
            elements_[element_inside_offset_] = element;

            element_inside_offset_++;
        }

        size_t IntersectOffset() const { return intersect_offset_; }
        size_t ElementInsideOffset() const { return element_inside_offset_; }
        const auto &Elements() const { return elements_; }

        bool IsGridInsideElement(size_t element_index) const { return element_index < intersect_offset_; }

        bool IsElementInsideGrid(size_t element_index) const
        {  // 用户确保在elements_.size()内
            return element_index >= element_inside_offset_;
        }

    private:
        size_t intersect_offset_{0};       // 第二段intersect的起始偏移
        size_t element_inside_offset_{0};  // 第三段element inside grid
        MeVector<Element> elements_;       // 实际存储的数组
    };

    void InsertElement(const Element &element, const BoxI &element_box, const BoxI &grid_box)
    {
        bool is_shape = !element.IsType(ElementType::kInstance);
        // 将格子中的元素分为 前（格子被element覆盖）、中（相交）、后（element被格子覆盖）三部分
        if (IsContain(grid_box, element_box)) {
            // 新增的element被网格全包含，直接放入末尾，并返回
            GetTripleRange(is_shape).InsertElementInside(element);
        } else if (IsContain(element_box, grid_box)) {
            GetTripleRange(is_shape).InsertGridInside(element);
        } else {
            GetTripleRange(is_shape).InsertIntersect(element);
        }
    }

    void InsertRepetitionElement(const Element &element, const BoxI &element_box, const BoxI &grid_box)
    {
        // 将格子中的元素分为 前（格子被repetion覆盖 = 0）、中（相交）、后（repetion被格子覆盖）两部分
        if (IsContain(grid_box, element_box)) {
            // 新增的element被网格全包含，直接放入末尾，并返回
            GetTripleRange(true).InsertElementInside(element);
        } else {
            GetTripleRange(true).InsertIntersect(element);
        }
    }

    const TripleRange &GetTripleRange(bool is_shape) const
    {
        if (is_shape) {
            return shapes_;
        } else {
            return instances_;
        }
    }

    TripleRange &GetTripleRange(bool is_shape)
    {
        if (is_shape) {
            return shapes_;
        } else {
            return instances_;
        }
    }

private:
    TripleRange shapes_;
    TripleRange instances_;
};

/**
 * @brief 通过网格实现空间索引功能，是一个静态网格，可复用于多个迭代器
 *
 * @tparam Element 图形类型
 */
class SpatialIndex {
public:
    /**
     * @brief 网格对象构造函数
     *
     * @param option   生成网格对象的一些配置参数
     * @param elements 需要加入网格对象中的图形集合
     */
    SpatialIndex(const SpatialIndexOption &option, const MeVector<Element> &elements) { InitGrid(option, elements); }

    ~SpatialIndex() = default;

    SpatialIndex(const SpatialIndex &) = default;
    SpatialIndex(SpatialIndex &&) = default;

    SpatialIndex &operator=(const SpatialIndex &) = default;
    SpatialIndex &operator=(SpatialIndex &&) = default;

    /**
     * @brief 获取网格参数
     *
     * @return 网格参数
     */
    const SpatialIndexOption &Option() const { return option_; }

    /**
     * @brief 在网格对象中查询 region 区域中的所有图形(与查询区域为 intersect 关系)
     *
     * @param region 查询区域
     * @param mode 查询模式
     * @param query_type 查询类型
     * @param out 查询结果集
     */
    void Query(const BoxI &region, SpatialQueryMode mode, QueryElementType query_type,
               MeUnorderedSet<Element> &out) const
    {
        if (rows_ == 0 || columns_ == 0) {
            return;
        }

        if (medb2::IsIntersect(region, grid_world_box_)) {
            if (mode == SpatialQueryMode::kSimple) {
                SimpleModeQuery(region, query_type, out);
            } else {
                AccurateModeQuery(region, query_type, out);
            }
        }
    }

    /**
     * @brief 计算网格row、column参数
     *
     * @param option 生成网格对象的一些配置参数
     * @param elements 需要加入网格对象中的图形集合
     * @return row、column网格参数
     */
    static std::pair<uint32_t, uint32_t> CalculcateRowColumn(SpatialIndexOption &option,
                                                             const MeVector<Element> &elements)
    {
        uint32_t row;
        uint32_t column;
        if (option.WindowStep() == 0) {
            row = (elements.size() < option.kNodeCapacity)
                      ? 1
                      : static_cast<uint32_t>(std::ceil(std::sqrt(elements.size() / option.kNodeCapacity)));
            row = std::min(row, option.kRowMax);
            row = std::max(row, option.kRowMin);

            column = row;  // 当前只支持 column = row，后续可变种
        } else {
            auto grid_region = GetGridSize(elements, option);
            row = std::max(
                static_cast<uint32_t>(std::ceil(static_cast<double>(grid_region.Height()) / option.WindowStep())), 1u);
            column = std::max(
                static_cast<uint32_t>(std::ceil(static_cast<double>(grid_region.Width()) / option.WindowStep())), 1u);
        }
        return std::make_pair(row, column);
    }

private:
    // 网格初始化
    void InitGrid(const SpatialIndexOption &option, const MeVector<Element> &elements)
    {
        option_ = option;
        if (elements.empty()) {
            return;
        }
        auto grid_region = GetGridSize(elements, option_);
        if (grid_region.Empty()) {
            return;
        }
        auto rows_columns = CalculcateRowColumn(option_, elements);
        rows_ = rows_columns.first;
        columns_ = rows_columns.second;
        if (option.WindowStep() == 0) {
            // 网格区域的宽高
            double grids_width = grid_region.Width();
            double grids_height = grid_region.Height();

            // 每个网格大小
            grid_width_ = static_cast<uint32_t>(std::ceil(grids_width / columns_));
            grid_height_ = static_cast<uint32_t>(std::ceil(grids_height / rows_));
        } else {
            // 每个网格大小
            grid_width_ = static_cast<uint32_t>(option.WindowStep());
            grid_height_ = grid_width_;
        }
        // polygon 数量 N，网格数 sqrt(N/10) x sqrt(N/10)
        // 每个网格大小 grid_region.x / rows_
        // 网格位置
        gridpos_xmin_ = grid_region.Left();
        gridpos_ymin_ = grid_region.Bottom();
        grid_.resize(rows_ * columns_);
        grid_world_box_ = BoxI(gridpos_xmin_, gridpos_ymin_, Accumulate(gridpos_xmin_, columns_ * grid_width_),
                               Accumulate(gridpos_ymin_, rows_ * grid_height_));
        InsertElements(elements);
    }

    /**
     * @brief 获取网格的 box,若没有则通过 elements 的极限 element 求取
     *
     * @param elements 需要加入网格对象中的图形集合
     * @param option 生成网格对象的一些配置参数
     * @return BoxI 待生成网格的整个区域
     */
    static BoxI GetGridSize(const MeVector<Element> &elements, SpatialIndexOption &option)
    {
        // 计算 elements 的 boundingbox(网格大小)
        if (option.Region().Empty() && !elements.empty()) {
            // 判断是否为 box,确定是否需要获取包围盒
            auto element_box_min = elements[0].BoundingBox(option.InstanceLayer());
            for (auto const &element : elements) {
                const BoxI &element_box = element.BoundingBox(option.InstanceLayer());
                BoxUnion(element_box_min, element_box);
            }
            option.SetRegion(element_box_min);
            return element_box_min;
        }
        return option.Region();
    }

    // 遍历 elements,放入对应的网格中
    void InsertElements(const MeVector<Element> &elements)
    {
        for (const auto &element : elements) {
            // 判断是否为 box,确定是否需要获取包围盒
            const auto element_box = element.BoundingBox(option_.InstanceLayer());
            if (element_box.Empty()) {
                continue;
            }

            // 计算 polygon 左下点所在的网格下标(int32_t 1/2 = 0)
            uint32_t row_min = Distance(element_box.Bottom(), gridpos_ymin_) / grid_height_;
            uint32_t column_min = Distance(element_box.Left(), gridpos_xmin_) / grid_width_;

            // 计算 polygon 右上点所在的网格下标
            uint32_t row_max = Distance(element_box.Top(), gridpos_ymin_) / grid_height_;
            uint32_t column_max = Distance(element_box.Right(), gridpos_xmin_) / grid_width_;
            // 判断是否在右上方边界,数组下标需减一
            row_max = ((row_max == rows_) ? (row_max - 1) : row_max);
            column_max = ((column_max == columns_) ? (column_max - 1) : column_max);
            for (int32_t i = static_cast<int32_t>(row_min); i <= static_cast<int32_t>(row_max); ++i) {
                for (int32_t j = static_cast<int32_t>(column_min); j <= static_cast<int32_t>(column_max); ++j) {
                    InsertElementsToGrid(element, i, j, element_box);
                }
            }
        }
    }

    void GridRange(const BoxI &region, SpatialQueryMode mode, BoxI &grid_range) const
    {
        // 查询区域
        int32_t query_region_xmin = std::max(region.Left(), grid_world_box_.Left());
        int32_t query_region_ymin = std::max(region.Bottom(), grid_world_box_.Bottom());
        int32_t query_region_xmax = std::min(region.Right(), grid_world_box_.Right());
        int32_t query_region_ymax = std::min(region.Top(), grid_world_box_.Top());

        int32_t difference = (mode == SpatialQueryMode::kSimple) ? 1 : 0;

        // 查询区域左下角以及右上角所在网格索引
        uint32_t column_min = Distance(query_region_xmin, gridpos_xmin_) / grid_width_;
        uint32_t row_min = Distance(query_region_ymin, gridpos_ymin_) / grid_height_;
        uint32_t column_max = Distance(query_region_xmax, (gridpos_xmin_ + difference)) / grid_width_;
        uint32_t row_max = Distance(query_region_ymax, (gridpos_ymin_ + difference)) / grid_height_;
        column_min = ((column_min == columns_) ? (column_min - 1) : column_min);
        row_min = ((row_min == rows_) ? (row_min - 1) : row_min);
        column_max = ((column_max == columns_) ? (column_max - 1) : column_max);
        row_max = ((row_max == rows_) ? (row_max - 1) : row_max);
        grid_range.Set({static_cast<int32_t>(column_min), static_cast<int32_t>(row_min)},
                       {static_cast<int32_t>(column_max), static_cast<int32_t>(row_max)});
    }

    void AccurateModeQuery(const BoxI &box, QueryElementType query_type, MeUnorderedSet<Element> &out) const
    {
        if (IsContain(box, grid_world_box_)) {
            // 如果region 包含了 world box，则将所有elements返回：
            for (int32_t i = 0; i < static_cast<int32_t>(rows_); ++i) {
                for (int32_t j = 0; j < static_cast<int32_t>(columns_); ++j) {
                    uint32_t index = static_cast<uint32_t>(i) * columns_ + static_cast<uint32_t>(j);
                    OutPutGridElements(index, query_type, out);
                }
            }
            return;
        }
        // 查询区域
        int32_t query_region_xmin = std::max(box.Left(), grid_world_box_.Left());
        int32_t query_region_ymin = std::max(box.Bottom(), grid_world_box_.Bottom());
        int32_t query_region_xmax = std::min(box.Right(), grid_world_box_.Right());
        int32_t query_region_ymax = std::min(box.Top(), grid_world_box_.Top());
        BoxI grid_range;
        GridRange(box, SpatialQueryMode::kAccurate, grid_range);
        for (int32_t i = grid_range.Bottom(); i <= grid_range.Top(); ++i) {
            for (int32_t j = grid_range.Left(); j <= grid_range.Right(); ++j) {
                uint32_t ui = static_cast<uint32_t>(i);
                uint32_t uj = static_cast<uint32_t>(j);
                uint32_t index = ui * columns_ + uj;
                // 当前网格的左下，右上坐标
                int32_t gridpos_xmin = Accumulate(gridpos_xmin_, uj * grid_width_);
                int32_t gridpos_ymin = Accumulate(gridpos_ymin_, ui * grid_height_);
                int32_t gridpos_xmax = Accumulate(gridpos_xmin, grid_width_);
                int32_t gridpos_ymax = Accumulate(gridpos_ymin, grid_height_);
                if (query_region_xmin <= gridpos_xmin && query_region_ymin <= gridpos_ymin &&
                    query_region_xmax >= gridpos_xmax && query_region_ymax >= gridpos_ymax) {
                    OutPutGridElements(index, query_type, out);
                    continue;
                }
                InsertIntersectElements(box, index, query_type, out);
            }
        }
    }

    void InsertIntersectElements(const BoxI &box, uint32_t index, QueryElementType query_type,
                                 MeUnorderedSet<Element> &out) const
    {
        if (query_type != QueryElementType::kOnlyInstance) {
            const auto &triple_range = grid_[index].GetTripleRange(true);
            InsertIntersectElements(box, triple_range.IntersectOffset(), triple_range.Elements(), out);
        }
        if (query_type != QueryElementType::kOnlyShape) {
            const auto &triple_range = grid_[index].GetTripleRange(false);
            InsertIntersectElements(box, triple_range.IntersectOffset(), triple_range.Elements(), out);
        }
    }

    void InsertIntersectElements(const BoxI &box, size_t inside_count, const MeVector<Element> &src,
                                 MeUnorderedSet<Element> &out) const
    {
        out.insert(src.begin(), src.begin() + inside_count);
        auto it = src.begin() + inside_count;
        for (; it != src.end(); ++it) {
            if (IsIntersect(*it, box)) {
                out.insert(*it);
            }
        }
    }

    // Init网格时将图形插入网格中
    void InsertElementsToGrid(const Element &element, uint32_t row, uint32_t col, const BoxI &element_box)
    {
        uint32_t index = row * columns_ + col;
        // 当前网格的左下，右上坐标
        int32_t gridpos_xmin = Accumulate(gridpos_xmin_, col * grid_width_);
        int32_t gridpos_ymin = Accumulate(gridpos_ymin_, row * grid_height_);
        int32_t gridpos_xmax = Accumulate(gridpos_xmin, grid_width_);
        int32_t gridpos_ymax = Accumulate(gridpos_ymin, grid_height_);
        const BoxI grid_box(gridpos_xmin, gridpos_ymin, gridpos_xmax, gridpos_ymax);
        if (element.IsType(ElementType::kBoxRep)) {
            InsertRepetitionToGrid<BoxI>(element, element_box, grid_box, index);
            return;
        }
        if (element.IsType(ElementType::kPolygonRep)) {
            InsertRepetitionToGrid<PolygonI>(element, element_box, grid_box, index);
            return;
        }
        grid_[index].InsertElement(element, element_box, grid_box);
    }

    /**
     * @brief 若 repetition 中有图形与格子相交，则将 repetition 加入格子。repetition 不会考虑是否全覆盖格子
     *
     * @param element 待加入网格的 Element，必须是 repetition 类型
     * @param grid_region 当前网格的区域
     * @param index 待插入网格的索引
     */
    template <class ShapeType>
    void InsertRepetitionToGrid(const Element &element, const BoxI &element_box, const BoxI &grid_region,
                                uint32_t index)
    {
        auto shape_ptr = element.Cast<ShapeRepetition<ShapeType>>();
        if (shape_ptr == nullptr || !shape_ptr->HasShapeIntersect(grid_region)) {
            return;
        }
        grid_[index].InsertRepetitionElement(element, element_box, grid_region);
    }

    /**
     * @brief 判断 element 是否与给定的 region 相交。对于 repetition 类型，会精确到匹配其中具体的图形。
     *
     * @param element 做区域相交检测的 Element 对象
     * @param region 区域相交检测的区域
     * @return 若 element 与指定区域相交，则返回 true，否则返回 false
     */
    bool IsIntersect(const Element &element, const BoxI &region) const
    {
        if (element.IsType(ElementType::kBoxRep)) {
            auto boxrp_ptr = element.Cast<ShapeRepetition<BoxI>>();
            return boxrp_ptr != nullptr && boxrp_ptr->HasShapeIntersect(region);
        } else if (element.IsType(ElementType::kPolygonRep)) {
            auto polyrp_ptr = element.Cast<ShapeRepetition<PolygonI>>();
            return polyrp_ptr != nullptr && polyrp_ptr->HasShapeIntersect(region);
        }

        const BoxI &element_box = element.BoundingBox(option_.InstanceLayer());
        return medb2::IsIntersect(element_box, region);
    }

    /**
     * @brief SIMPLE模式 通过网格索引得到相应网格中的图形
     *
     * @param region 查询区域
     * @param grid_world_box 整个网格的box
     * @param out 查询结果
     */
    void SimpleModeQuery(const BoxI &region, QueryElementType query_type, MeUnorderedSet<Element> &out) const
    {
        BoxI grid_range;
        GridRange(region, SpatialQueryMode::kSimple, grid_range);
        for (int32_t i = grid_range.Bottom(); i <= grid_range.Top(); ++i) {
            for (int32_t j = grid_range.Left(); j <= grid_range.Right(); ++j) {
                uint32_t index = static_cast<uint32_t>(i) * columns_ + static_cast<uint32_t>(j);
                OutPutGridElements(index, query_type, out);
            }
        }
    }

    void OutPutGridElements(uint32_t index, QueryElementType query_type, MeUnorderedSet<Element> &out) const
    {
        if (query_type != QueryElementType::kOnlyInstance) {
            const auto &triple_range = grid_[index].GetTripleRange(true);
            out.insert(triple_range.Elements().begin(), triple_range.Elements().end());
        }
        if (query_type != QueryElementType::kOnlyShape) {
            const auto &triple_range = grid_[index].GetTripleRange(false);
            out.insert(triple_range.Elements().begin(), triple_range.Elements().end());
        }
    }

private:
    uint32_t rows_{0};         // 网格,行
    uint32_t columns_{0};      // 网格,列
    MeVector<GridNode> grid_;  // 网格数组,用一个 vector 代表二维数组
    uint32_t grid_width_{0};   // 单个网格宽
    uint32_t grid_height_{0};  // 单个网格高
    int32_t gridpos_xmin_{0};  // 网格左下角位置X
    int32_t gridpos_ymin_{0};  // 网格左下角位置Y
    SpatialIndexOption option_;
    BoxI grid_world_box_;

    friend class SpatialIterator;
};

}  // namespace medb2

#endif  // MEDB_SPATIAL_INDEX_H
