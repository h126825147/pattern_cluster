/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_ELEMENT_ITERATOR_H
#define MEDB_ELEMENT_ITERATOR_H

#include <stack>
#include <variant>
#include "medb/cell.h"
#include "medb/cell_element_iterator.h"
#include "medb/element.h"
#include "medb/enums.h"
#include "medb/layer.h"

namespace medb2 {

/**
 * 构造 ElementIterator 的配置参数
 */
class ElementIteratorOption {
public:
    ElementIteratorOption(const Cell &cell, const Layer &layer) : cell_ptr_(&cell), query_layer_(layer) {}
    void SetCellPtr(const Cell &cell) { cell_ptr_ = &cell; }
    const Cell *CellPtr() const { return cell_ptr_; }

    void SetQueryLayer(const Layer &layer) { query_layer_ = layer; }
    const Layer &QueryLayer() const { return query_layer_; }

    void SetRegion(const BoxI &region) { region_ = region; }
    const BoxI &Region() const { return region_; }
    bool HasRegion() const { return !region_.Empty(); }

    void SetMaxLevel(uint32_t max_level) { max_level_ = max_level; }
    uint32_t MaxLevel() const { return max_level_; }

    void SetType(QueryElementType type) { type_ = type; }
    QueryElementType Type() const { return type_; }

    void SetMode(SpatialQueryMode mode) { mode_ = mode; }
    SpatialQueryMode Mode() const { return mode_; }

    void SetNeedPolygonData(bool need_polygon_data) { need_polygon_data_ = need_polygon_data; }
    bool NeedPolygonData() const { return need_polygon_data_; }
    bool IsPolygonData() const { return need_polygon_data_ && type_ == QueryElementType::kOnlyShape; }

private:
    const Cell *cell_ptr_{nullptr};  // 指定 Cell 进行访问
    Layer query_layer_;              // 仅访问此 Layer 上的 Element
    BoxI region_;                    // 仅访问此区域内的 Element，若区域为空，则表示不限定区域
    uint32_t max_level_{std::numeric_limits<uint32_t>::max()};    // 最大访问层级，输入 cell 的层级为 0
    QueryElementType type_{QueryElementType::kShapeAndInstance};  // 指定访问哪些类型的 Element
    SpatialQueryMode mode_{SpatialQueryMode::kAccurate};  // 仅在存在空间索引的情况下有效，表示按哪个模式获取 Element
    bool need_polygon_data_{false};  // 指示是否获取 PolygonDataI 数据，仅在 kOnlyShape 时有效，获取到的是打平后的数据
};

/**
 * @brief Element 迭代器，用以访问指定 Cell 上的 Element，可通过 Layer、矩形区域、Cell 层级、Element
 * 类型等信息进行过滤。以前序方式对 Cell 进行遍历
 *
 */
class ElementIterator {
public:
    /**
     * @brief 构造迭代器。此迭代器可访问指定 Cell 上的 Element，可通过 Layer、矩形区域、Cell 层级、Element
     * 类型等信息进行过滤
     *
     * @param option 迭代器参数集合
     */
    explicit ElementIterator(const ElementIteratorOption &option) : option_(option)
    {
        if (!option_.HasRegion()) {
            return;
        }

        const auto bbox = option_.CellPtr()->GetBoundingBox(option_.QueryLayer());
        if (IsContain(option_.Region(), bbox)) {
            option_.SetRegion(BoxI());  // 退化为不带区域的普通遍历，以加速性能
        }
    }

    ElementIterator(const ElementIterator &) = delete;
    ElementIterator &operator=(const ElementIterator &) = delete;
    ~ElementIterator() = default;

    /**
     * @brief 将迭代器移动到第一个有效 Element 上，若不存在，则移动到末尾。必须 Begin 后才能使用后续接口
     */
    void Begin() { BeginCell(); }

    /**
     * @brief 步进迭代器，使迭代器迭代到下一个位置，可能为末尾，未到达末尾才能正确获取数据
     */
    void Next()
    {
        if (IsEnd()) {
            return;
        }
        NextCell();
    }

    /**
     * @brief 判断迭代器是否已经指向结束位置
     *
     * @return 结尾则返回 True, 否则返回 False
     */
    bool IsEnd() const { return nodes_.empty(); }

    /**
     * @brief 获取当前迭代器指向的 Element 及其对应的 Transformation 信息。
     * 注意，当获取 PolygonData 功能生效时，此接口无法获取数据
     *
     * @param current_element 输出参数，用于接受当前迭代器指向的 Element 对象数据
     * @param current_trans 输出参数，用于接受当前 Element 的 Transformation 信息
     * @return 成功获取则返回 true，否则返回 false
     */
    bool Current(Element &current_element, TransformationVar &current_trans) const
    {
        if (option_.IsPolygonData() || IsEnd()) {
            return false;
        }
        current_element = cell_element_it_->Current();
        current_trans = CurrentTrans();
        return true;
    }

    /**
     * @brief 将迭代器当前指向的元素转换为 PolygonDataI 对象并返回。
     * 注意：仅在 kOnlyShape 及 need_polygon_data_ 为 true 时生效。未生效时无法获取数据
     *
     * @return 当前迭代器指向元素对应的 PolygonDataI 对象。
     */
    PolygonDataI CurrentPolygonData() const
    {
        if (!option_.IsPolygonData() || IsEnd()) {
            return {};
        }

        // 如果 repetition 中有数据，则先返回其数据
        auto element = cell_element_it_->Current();
        if (IsRepetitionType(element)) {
            return std::visit([this](auto &&repetition) { return ToPolygonData(repetition.GetShape(repetition_idx_)); },
                              repetition_);
        }

        return std::visit(
            [&element](auto &&trans) {
                if (element.IsType(ElementType::kBox)) {
                    return ToPolygonData(element.Cast<BoxI>()->Transformed(trans));
                } else if (element.IsType(ElementType::kPolygon)) {
                    return ToPolygonData(element.Cast<PolygonI>()->Transformed(trans));
                } else if (element.IsType(ElementType::kPath)) {
                    return ToPolygonData(element.Cast<PathI>()->Transformed(trans));
                }
                return PolygonDataI();
            },
            CurrentTrans());
    }

    /**
     * @brief 返回当前迭代到的层级，入参 Cell 为第 0 层
     *
     * @return 当前迭代的层级
     */
    uint32_t CurrentLevel() const { return static_cast<uint32_t>(nodes_.empty() ? 0 : (nodes_.size() - 1)); };

    /**
     * @brief 获取当前 Element 的 Transformation 信息
     *
     * @return 当前 Element 的 Transformation 信息
     */
    const TransformationVar CurrentTrans() const { return nodes_.empty() ? TransformationVar() : nodes_.top().trans_; };

    /**
     * @brief 获取当前 Element 所属的 Cell
     *
     * @return 当前 Element 所属的 Cell
     */
    const Cell *CurrentCell() const { return nodes_.empty() ? nullptr : nodes_.top().cell_; };

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    class StackNode {
    public:
        const Cell *cell_{nullptr};        // 当前 Cell
        const TransformationVar trans_;    // 在根 cell 上的 transform
        CellElementIterator instance_it_;  // cell_ 的 Instances 迭代器，可复用空间索引
        size_t placement_idx_{0};          // 下一个子 cell 在 placement 中的索引，max 表示 cell_ 自身未迭代
        StackNode(const Cell *cell, const TransformationVar &t, const Layer &layer, const BoxI &region)
            : cell_(cell),
              trans_(t),
              instance_it_(*cell, layer, QueryElementType::kOnlyInstance, region, SpatialQueryMode::kAccurate)
        {
            instance_it_.Begin();
        }
        USE_MEDB_OPERATOR_NEW_DELETE
    };

    bool BeginCell()
    {
        nodes_ = {};
        nodes_.emplace(option_.CellPtr(), SimpleTransformation(), option_.QueryLayer(), option_.Region());
        if (BeginElement()) {
            return true;
        }
        return NextCell();
    }

    /**
     * @brief 将迭代器移动到下一个有效 Element 上，若不存在，则移动到末尾。
     * 注意需要剪枝，有如下 5 种情况可剪枝
     * 剪枝1. 超过 max_level
     * 剪枝2. Cell 在指定的 Layer 上无数据(通过 Cell::GetBoundBox(layer) 是否为空判断)
     * 剪枝3. Cell 在指定的 Layer 上的数据不在指定区域中（Cell::GetBoundBox(layer) 与指定区域不相交）
     * 剪枝4. 在 1,2,3 都不成立的情况下，且仅查询 Instance ，Cell 上不存在 Instance(仅 Cell 自己的 Shapes 在区域中)
     * 剪枝5. 在 1,2,3 都不成立的情况下，需要 Instance，但通过 CellElementIterator 查询不到（Cell 虽与指定区域相交但是无
     * Element 在区域内），剪枝所有子 Cell
     */
    bool NextCell()
    {
        if (NextElement()) {
            return true;
        }

        // 更新节点信息
        while (!IsEndCell()) {
            auto &top = nodes_.top();
            if (!AddNextCell(top)) {
                nodes_.pop();
                continue;
            }
            if (BeginElement()) {
                return true;
            } else if (IsNeedInstance()) {
                nodes_.pop();  // 剪枝5
            }
        }
        return false;
    }

    bool AddNextCell(StackNode &node)
    {
        if (nodes_.size() > option_.MaxLevel()) {  // 剪枝1
            return false;
        }

        for (; !node.instance_it_.IsEnd(); node.instance_it_.Next()) {
            const auto instance = node.instance_it_.Current().Cast<Instance>();
            const auto cell = instance->CellPtr();
            // 如果 SREF 或者 AREF 里第一个元素，则判断剪枝4
            if (node.placement_idx_ == 0) {
                if (option_.Type() == QueryElementType::kOnlyInstance && cell->Instances().empty()) {  // 剪枝4
                    continue;
                }
            }

            const auto placement_size = instance->PlacementPtr()->Size();
            const auto placement = instance->PlacementPtr();
            // SREF，node.placement_idx_ 必为 0
            if (placement_size == 1) {
                node.instance_it_.Next();
                auto trans = medb2::Compose(node.trans_, placement->Trans(0));
                nodes_.emplace(cell, trans, option_.QueryLayer(),
                               option_.HasRegion() ? GetInvertedRegion(trans) : BoxI());
                return true;
            };

            // AREF
            while (node.placement_idx_ < placement_size) {
                auto trans = medb2::Compose(node.trans_, placement->Trans(node.placement_idx_));
                node.placement_idx_++;
                if (!option_.HasRegion()) {
                    nodes_.emplace(cell, trans, option_.QueryLayer(), BoxI());
                    return true;
                }

                const auto box = cell->GetBoundingBox(option_.QueryLayer());
                const auto currrent_region = GetInvertedRegion(trans);
                if (!IsIntersect(box, currrent_region)) {  // 剪枝3
                    continue;
                }
                nodes_.emplace(cell, trans, option_.QueryLayer(), currrent_region);
                return true;
            }

            node.placement_idx_ = 0;
        }
        return false;
    }

    bool IsEndCell() { return nodes_.empty(); }

    /**
     * 开始对当前栈顶 Cell 进行 Element 迭代
     */
    bool BeginElement()
    {
        ResetCellElementIterator(*CurrentCell(), option_.HasRegion() ? GetInvertedRegion(CurrentTrans()) : BoxI());

        cell_element_it_->Begin();
        if (IsEndElement()) {
            return false;
        }

        if (BeginRepetitionShape()) {
            return true;
        }

        return NextElement();
    }

    bool NextElement()
    {
        if (NextRepetitionShape()) {
            return true;
        }

        while (!IsEndElement()) {
            cell_element_it_->Next();
            if (IsEndElement()) {
                return false;
            }

            if (BeginRepetitionShape()) {
                return true;
            }
        }
        return false;
    }

    bool IsEndElement() { return cell_element_it_->IsEnd(); }

    void ResetCellElementIterator(const Cell &cell, const BoxI &region)
    {
        // 当 cell 不在 region 内时，CellElementIterator 中会优化为直接到达末尾
        if (cell_element_it_) {
            *cell_element_it_ = CellElementIterator(cell, option_.QueryLayer(), option_.Type(), region, option_.Mode());
        } else {
            cell_element_it_ = std::make_unique<CellElementIterator>(cell, option_.QueryLayer(), option_.Type(), region,
                                                                     option_.Mode());
        }
    }

    bool IsNeedInstance() { return option_.Type() != QueryElementType::kOnlyShape; }

    BoxI GetInvertedRegion(const TransformationVar &trans) const
    {
        return std::visit([this](auto &&t) { return option_.Region().Transformed(t.Inverted()); }, trans);
    }

    /**
     * 开始对当前 Element 进行 repetition 元素迭代
     * 仅在需要输出 PolygonData，且当前 Element 是 ShapeRepetition 且其中无元素在指定区域内时返回 false
     */
    bool BeginRepetitionShape()
    {
        if (!option_.IsPolygonData()) {
            return true;  // 因为只要element，repetition当做一个整体返回，不需要处理
        }

        repetition_idx_ = 0;
        repetition_size_ = 1;  // 单个图形当作只有一个元素的repetition

        auto element = cell_element_it_->Current();  // 调用者保证element有效
        if (element.IsType(ElementType::kBoxRep)) {
            return BeginRepetitionShape(*element.Cast<ShapeRepetition<BoxI>>());
        } else if (element.IsType(ElementType::kPolygonRep)) {
            return BeginRepetitionShape(*element.Cast<ShapeRepetition<PolygonI>>());
        }

        return true;
    }

    template <class ShapeType>
    bool BeginRepetitionShape(const ShapeRepetition<ShapeType> &shape)
    {
        return std::visit(
            [this, &shape](auto &&trans) {
                auto transformed_shape = shape.Transformed(trans);
                size_t idx = 0;
                if (option_.HasRegion()) {
                    idx = transformed_shape.FindIntersected(option_.Region(), 0);
                    if (idx >= shape.Size()) {
                        return false;
                    }
                }
                repetition_ = std::move(transformed_shape);
                repetition_size_ = shape.Size();
                repetition_idx_ = idx;
                return true;
            },
            CurrentTrans());
    }

    /**
     * 只有迭代到下一个有效的 repetition 元素时返回 true
     */
    bool NextRepetitionShape()
    {
        ++repetition_idx_;
        if (repetition_idx_ >= repetition_size_) {
            return false;
        }

        if (!option_.HasRegion()) {
            return true;
        }

        repetition_idx_ = std::visit(
            [this](auto &&shape) { return shape.FindIntersected(option_.Region(), repetition_idx_); }, repetition_);

        return repetition_idx_ < repetition_size_;
    }

    bool IsRepetitionType(const Element &element) const
    {
        return element.IsType(ElementType::kBoxRep) || element.IsType(ElementType::kPolygonRep);
    }

    ElementIteratorOption option_;
    std::unique_ptr<CellElementIterator> cell_element_it_;
    MeStack<StackNode> nodes_;  // 深度搜索的栈，其 size - 1 即当前的 level
    std::variant<ShapeRepetition<BoxI>, ShapeRepetition<PolygonI>> repetition_;
    size_t repetition_size_{0};
    size_t repetition_idx_{0};
};
}  // namespace medb2
#endif  // MEDB_ELEMENT_ITERATOR_H
