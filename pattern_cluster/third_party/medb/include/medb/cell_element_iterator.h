/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_CELL_ELEMENT_ITERATOR_H
#define MEDB_CELL_ELEMENT_ITERATOR_H

#include "medb/cell.h"
#include "medb/element.h"
#include "medb/enums.h"
#include "medb/layer.h"
#include "medb/spatial_iterator.h"

namespace medb2 {

/**
 * @brief 用以遍历单个 Cell 上的 Element
 */
class CellElementIterator {
public:
    static constexpr size_t kShapesSpatialIndexThreshold = 100;     // shapes 数量超过此阈值，则可使用空间索引
    static constexpr size_t kInstancesSpatialIndexThreshold = 100;  // instance 数量超过此阈值，则可使用空间索引

    /**
     * @brief 对指定 Cell 的指定 Layer 上的 Element 进行迭代，无空间查询
     *
     * @param cell 指定 Cell 进行访问
     * @param layer 仅访问此 Layer 上的 Element
     * @param type 指定访问哪些类型的 Element
     */
    CellElementIterator(const Cell &cell, const Layer &layer, QueryElementType type)
        : CellElementIterator(cell, layer, type, {}, SpatialQueryMode::kAccurate)
    {
    }

    /**
     * @brief 对指定 Cell 的指定 Layer 上的 Element 进行迭代。若 region 不为空，则只会迭代 region 内的 Element
     *
     * @param cell 指定 Cell 进行访问
     * @param layer 仅访问此 Layer 上的 Element
     * @param region 仅访问此区域内的 Element，若区域为空，则表示不限定区域
     * @param type 指定访问哪些类型的 Element
     * @param mode 仅在存在空间索引的情况下有效，表示按哪个模式获取 Element
     */
    CellElementIterator(const Cell &cell, const Layer &layer, QueryElementType type, const BoxI &region,
                        SpatialQueryMode mode)
        : cell_(&cell), layer_(layer), type_(type)
    {
        shapes_ = IsNeedShapes() ? &cell.GetShapes(layer) : nullptr;
        if (region.Empty() || IsContain(region, cell.GetBoundingBox(layer))) {
            return;  // 无区域查询或者区域包含了整个 cell，则直接遍历
        }

        region_ = region;
        auto cell_cache_it = cell.cache_.find(layer);
        if (cell_cache_it == cell.cache_.end() || cell_cache_it->second.SpatialIndexDirty()) {
            return;  // 未创建空间索引，则使用全遍历的区域查询
        }

        // kSimple 模式时走空间索引，以便按空间索引的网格进行查询
        if (IsNeedShapes() &&
            (mode == SpatialQueryMode::kSimple || shapes_->Size(false) > kShapesSpatialIndexThreshold)) {
            shapes_spatial_iterator_ = std::make_unique<SpatialIterator>(*cell_cache_it->second.SpatialIndexPtr(),
                                                                         region_, mode, QueryElementType::kOnlyShape);
        }
        if (IsNeedInstance() &&
            (mode == SpatialQueryMode::kSimple || cell_->Instances().size() > kInstancesSpatialIndexThreshold)) {
            instances_spatial_iterator_ = std::make_unique<SpatialIterator>(
                *cell_cache_it->second.SpatialIndexPtr(), region_, mode, QueryElementType::kOnlyInstance);
        }
    }

    /**
     * @brief 将迭代器移动到第一个有效 Element 上，若不存在，则移动到末尾。必须 Begin 后才能使用后续接口
     */
    void Begin()
    {
        if (!region_.Empty() && !IsIntersect(region_, cell_->GetBoundingBox(layer_))) {
            shapes_type_idx_ = EToI(ElementType::kShapeNum);
            shape_idx_ = std::numeric_limits<size_t>::max();
            instance_idx_ = std::numeric_limits<size_t>::max();
            return;
        }

        if (IsNeedShapes() && BeginShapes()) {
            return;
        }

        if (IsNeedInstance()) {
            BeginInstances();
        }
    }

    /**
     * @brief 将迭代器移动到下一个有效 Element 上，若不存在，则移动到末尾。
     */
    void Next()
    {
        if (IsNeedShapes() && !IsShapesEnd()) {
            if (NextShape()) {
                return;
            }

            if (IsNeedInstance()) {
                BeginInstances();
            }
            return;
        }

        if (IsNeedInstance() && !IsInstanceEnd()) {
            NextInstance();
        }
    }

    /**
     * @brief 检测迭代器是否已经移动到末尾
     *
     * @return 若已移动到末尾，则返回 true，否则返回 false
     */
    bool IsEnd() const { return current_.Empty(); };

    /**
     * @brief 获取当前迭代器指向的 Element。若此时 IsEnd 为 true，则此接口返回的值可能非预期
     *
     * @return Element 当前迭代器指向的 Element 对象
     */
    Element Current() const { return current_; };

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    bool IsShapesOnSpatialIndex() { return shapes_spatial_iterator_ != nullptr; }
    bool IsInstanceOnSpatialIndex() { return instances_spatial_iterator_ != nullptr; }
    bool IsNeedShapes() { return type_ != QueryElementType::kOnlyInstance; }
    bool IsNeedInstance() { return type_ != QueryElementType::kOnlyShape; }
    bool IsShapesEnd()
    {
        return IsShapesOnSpatialIndex() ? shapes_spatial_iterator_->IsEnd()
                                        : (shapes_type_idx_ >= EToI(ElementType::kShapeNum));
    }
    bool IsInstanceEnd()
    {
        return IsInstanceOnSpatialIndex() ? instances_spatial_iterator_->IsEnd()
                                          : (instance_idx_ >= cell_->Instances().size());
    }

    // 调用者需保证在需要 Shapes 的情况才调用此函数
    bool BeginShapes()
    {
        if (!region_.Empty() && !IsIntersect(region_, shapes_->BoundingBox())) {
            shapes_type_idx_ = EToI(ElementType::kShapeNum);
            shape_idx_ = std::numeric_limits<size_t>::max();
            return false;
        }

        if (IsShapesOnSpatialIndex()) {
            shapes_spatial_iterator_->Begin();
            return UpdateCurrentForShapesOnSpatialIndex();
        }

        shape_idx_ = 0;
        shapes_type_idx_ = 0;
        return UpdateCurrentForShapes();
    }

    // 调用者需保证 shapes 未 end 才能调用此函数
    bool NextShape()
    {
        if (IsShapesOnSpatialIndex()) {
            shapes_spatial_iterator_->Next();
            return UpdateCurrentForShapesOnSpatialIndex();
        }

        shape_idx_++;
        return UpdateCurrentForShapes();
    }

    bool UpdateCurrentForShapesOnSpatialIndex()
    {
        if (shapes_spatial_iterator_->IsEnd()) {
            current_.Reset();
            return false;
        }

        current_ = shapes_spatial_iterator_->GetObj();
        return true;
    }

    bool UpdateCurrentForShapes()
    {
        for (; shapes_type_idx_ < EToI(ElementType::kShapeNum); ++shapes_type_idx_) {
            bool is_update_success = false;
            switch (static_cast<ElementType>(shapes_type_idx_)) {
                case ElementType::kBox:
                    is_update_success = UpdateCurrentForShapes<BoxI>();
                    break;
                case ElementType::kPolygon:
                    is_update_success = UpdateCurrentForShapes<PolygonI>();
                    break;
                case ElementType::kPath:
                    is_update_success = UpdateCurrentForShapes<PathI>();
                    break;
                case ElementType::kBoxRep:
                    is_update_success = UpdateCurrentForShapes<ShapeRepetition<BoxI>>();
                    break;
                case ElementType::kPolygonRep:
                    is_update_success = UpdateCurrentForShapes<ShapeRepetition<PolygonI>>();
                    break;
                default:
                    break;
            }
            if (is_update_success) {
                return true;
            }
            shape_idx_ = 0;
        }
        current_.Reset();
        return false;
    }

    template <class ShapeType>
    bool UpdateCurrentForShapes()
    {
        auto const &shapes = shapes_->Raw<ShapeType>();
        for (; shape_idx_ < shapes.size(); shape_idx_++) {
            auto const &shape = shapes[shape_idx_];
            if (region_.Empty()) {
                current_ = Element(&shape, ShapeTraits<ShapeType>::enum_value_);
                return true;
            }

            if constexpr (std::is_same_v<ShapeRepetition<BoxI>, ShapeType> ||
                          std::is_same_v<ShapeRepetition<PolygonI>, ShapeType>) {
                if (shape.HasShapeIntersect(region_)) {
                    current_ = Element(&shape, ShapeTraits<ShapeType>::enum_value_);
                    return true;
                }
            } else {
                if (IsIntersect(shape.BoundingBox(), region_)) {
                    current_ = Element(&shape, ShapeTraits<ShapeType>::enum_value_);
                    return true;
                }
            }
        }
        return false;
    }

    // 若需要 Instance 类型，则重置 Instance 类型的参数，并当 Shapes 类型迭代完时，将 Instance 迭代到下一个有效的
    // Element 上或者结束位置
    bool BeginInstances()
    {
        if (IsInstanceOnSpatialIndex()) {
            instances_spatial_iterator_->Begin();
            return UpdateCurrentForInstanceOnSpatialIndex();
        }

        instance_idx_ = 0;
        return UpdateCurrentForInstance(cell_->Instances());
    }

    // 由调用者保证 Instance 未 end 才能进入此函数
    bool NextInstance()
    {
        if (IsInstanceOnSpatialIndex()) {
            instances_spatial_iterator_->Next();
            return UpdateCurrentForInstanceOnSpatialIndex();
        }

        instance_idx_++;
        return UpdateCurrentForInstance(cell_->Instances());
    }

    bool UpdateCurrentForInstanceOnSpatialIndex()
    {
        if (instances_spatial_iterator_->IsEnd()) {
            current_.Reset();
            return false;
        }

        current_ = instances_spatial_iterator_->GetObj();
        return true;
    }

    bool UpdateCurrentForInstance(const MeVector<Instance> &insts)
    {
        if (region_.Empty()) {
            for (; instance_idx_ < insts.size(); ++instance_idx_) {
                if (!insts[instance_idx_].GetBoundingBox(layer_).Empty()) {  // 过滤掉仅含 Text 的 Layer
                    current_ = Element(&insts[instance_idx_], ElementType::kInstance);
                    return true;
                }
            }
        } else {
            for (; instance_idx_ < insts.size(); ++instance_idx_) {
                const BoxI &inst_box = insts[instance_idx_].GetBoundingBox(layer_);
                if (inst_box.Empty() || !IsIntersect(inst_box, region_)) {
                    continue;
                }
                current_ = Element(&insts[instance_idx_], ElementType::kInstance);
                return true;
            }
        }
        current_.Reset();
        return false;
    }

    const Cell *cell_{nullptr};
    Layer layer_;
    BoxI region_;
    QueryElementType type_{QueryElementType::kShapeAndInstance};
    const Shapes *shapes_{nullptr};  // 缓存 Shapes 对象，以避免重复查找

    size_t shapes_type_idx_{0};  // 当前 shapes type 的索引
    size_t shape_idx_{0};        // 当前 shape 的索引，即 MeVector<BoxI> MeVector<PolygonI> 等的索引
    size_t instance_idx_{0};     // 当前 instance 的索引
    std::unique_ptr<SpatialIterator>
        shapes_spatial_iterator_;  // 在带区域、cell 存在空间索引、shapes 数量足够多的场景下使用
    std::unique_ptr<SpatialIterator>
        instances_spatial_iterator_;  // 在带区域、cell 存在空间索引、instance 数量足够多的场景下使用

    Element current_;  // 对外输出的对象
};

}  // namespace medb2
#endif  // MEDB_CELL_ELEMENT_ITERATOR_H
