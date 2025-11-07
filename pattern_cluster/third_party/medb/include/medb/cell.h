/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_CELL_H
#define MEDB_CELL_H

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "medb/allocator.h"
#include "medb/element.h"
#include "medb/geometry_data.h"
#include "medb/instance.h"
#include "medb/layer.h"
#include "medb/shapes.h"
#include "medb/spatial_index.h"

namespace medb2 {
class MEDB_API CellUpdateOption {
public:
    CellUpdateOption() = default;
    CellUpdateOption(const std::vector<Layer> &layers, const SpatialIndexOption &spatial_index_option)
        : layers_(layers), updated_spatial_index_option_(spatial_index_option)
    {
    }

    CellUpdateOption(const std::vector<Layer> &layers, bool is_empty_spatial_index)
        : layers_(layers), is_empty_spatial_index_(is_empty_spatial_index)
    {
    }

    const std::vector<Layer> &Layers() const { return layers_; }
    const SpatialIndexOption &UpdatedSpatialIndexOption() const { return updated_spatial_index_option_; }
    bool IsEmptySpatialIndex() const { return is_empty_spatial_index_; }

private:
    std::vector<Layer> layers_;
    SpatialIndexOption updated_spatial_index_option_;
    bool is_empty_spatial_index_{false};  // 创建空的网格索引，来解决map多线程资源竞争。
};

class MEDB_API Cell {
public:
    using ShapesMap = MeMap<Layer, Shapes>;

    static constexpr uint8_t kInvalidMaxLevel = std::numeric_limits<uint8_t>::max();

    Cell() = default;
    // 对子 Cell 的 parent_cells_ 和父 Cell 的 instance_ 都进行了操作，线程不安全
    ~Cell()
    {
        for (const auto &inst : instances_) {
            inst.CellPtr()->parent_cells_.erase(this);
        }

        for (auto &parent : parent_cells_) {
            auto &parent_inst = parent->instances_;
            parent_inst.erase(std::remove_if(parent_inst.begin(), parent_inst.end(),
                                             [&](const Instance &inst) { return inst.CellPtr() == this; }),
                              parent_inst.end());
        }

        SetDirty();
        ClearMaxLevel();
    }

    explicit Cell(const std::string &name) : name_(name) {}

    Cell(const Cell &c) = default;
    Cell(Cell &&c) = default;

    Cell &operator=(const Cell &c) = default;
    Cell &operator=(Cell &&c) = default;

    /**
     * @brief 获取当前 Cell 对象所拥有的 layers, 不包含子 Cell
     *
     * @return std::vector<Layer> 所有 layers
     */
    std::vector<Layer> Layers() const
    {
        std::vector<Layer> out;
        std::for_each(shapes_map_.begin(), shapes_map_.end(),
                      [&out](const auto &pair) { out.emplace_back(pair.first); });
        return out;
    };

    /**
     * @brief 获取所有 layer，包含子 Cell
     *
     * @return std::vector<Layer> 所有 layers
     */
    std::vector<Layer> LayersIncludeChildren() const
    {
        if (!layer_dirty_.LayerFlag()) {
            return {layer_cache_.begin(), layer_cache_.end()};
        }

        layer_cache_.clear();
        MeSet<Layer> layers_set;
        for (auto const &inst : instances_) {
            auto cell_layers = inst.CellPtr()->LayersIncludeChildren();
            layers_set.insert(cell_layers.begin(), cell_layers.end());
        }
        for (auto const &[layer, _] : shapes_map_) {
            layers_set.emplace(layer);
        }

        std::copy(layers_set.begin(), layers_set.end(), std::back_inserter(layer_cache_));
        layer_dirty_.SetLayerFlag(false);

        // 使用双指针法清理不存在的Layer的缓存
        auto it_layer = layer_cache_.begin();
        auto it_cache = cache_.begin();
        while (it_cache != cache_.end()) {
            if (it_layer == layer_cache_.end() || it_cache->first < *it_layer) {
                it_cache = cache_.erase(it_cache);
            } else if ((it_cache->first == *it_layer)) {
                it_layer++;
                it_cache++;
            } else {  // >
                it_layer++;
            }
        }
        return {layer_cache_.begin(), layer_cache_.end()};
    }

    /**
     * @brief 获取当前 Cell 的所有父 Cell, 只读
     *
     * @return MeUnorderedSet<Cell *> 当前 Cell 的父 Cell 的指针的 vector 容器
     */
    const MeUnorderedSet<Cell *> &ParentCells() const { return parent_cells_; }

    /**
     * @brief 获取当前 Cell 的所有父 Cell
     *
     * @return MeUnorderedSet<Cell *> 当前 Cell 的父 Cell 的指针的 vector 容器
     */
    MeUnorderedSet<Cell *> &ParentCells() { return parent_cells_; }

    /**
     * @brief 返回子cell出现的次数
     *
     * @param child 待统计的子cell
     * @return size_t 子cell出现的次数
     */
    size_t GetChildCount(const Cell &child)
    {
        size_t total_count = 0;
        for (const auto &inst : instances_) {
            const auto *cell = inst.CellPtr();
            if (cell == nullptr) {
                continue;
            }
            if (cell == &child) {
                total_count += inst.PlacementPtr()->Size();
            }
        }
        return total_count;
    }

    /**
     * @brief 判断当前 cell 是否为空
     */
    bool Empty() const { return shapes_map_.empty() && instances_.empty(); }

    /**
     * @brief 清空所有的 图形和Instances, 会修改子 Cell 的 parent_cells_, 线程不安全
     */
    void Clear()
    {
        shapes_map_.clear();
        SetDirty();
        ClearInstances();
    }

    /**
     * @brief 返回当前 Cell 对象中指定 layers 上的图形，不包含子 Cell
     *
     * @param layer 指定的 layer
     * @return Shapes& 获取到的图形集
     */
    const Shapes &GetShapes(const Layer &layer) const
    {
        auto it = shapes_map_.find(layer);
        if (it == shapes_map_.end()) {
            static const Shapes null;
            return null;
        }
        return it->second;
    }

    /**
     * @brief 返回当前 Cell 对象中指定 layers 上的图形，不包含子 Cell。
     *        注意，若指定的 Layer 不存在则创建此 Layer 并返回一个空的 Shapes
     *
     * @param layer 指定的 layer
     * @return Shapes& 获取到的图形集
     */
    Shapes &GetShapesForWrite(const Layer &layer)
    {
        SetDirty(kAllDirty, &layer, 1);
        return shapes_map_[layer];
    }

    /**
     * @brief 给当前 Cell 对应 Layer 新增一个图形，若当前 Cell 无 Shapes，新增 Shapes 再添加图形
     *
     * @param layer 需要新增图形的 layer
     * @param shape 需要新增的图形(box, path, polygon, text, Repetition)，可左值可右值
     */
    template <class ShapeType>
    void InsertShape(const Layer &layer, ShapeType &&shape)
    {
        auto &cell_shapes = shapes_map_[layer];
        cell_shapes.Insert(std::forward<ShapeType>(shape));
        SetDirty();
    }

    /**
     * @brief 替换指定 shape 的数据，用于对某个 shape 的修改操作。此操作不会修改原对象的内存地址
     *
     * @tparam ShapeType BoxI/PolygonI/PathI/Text/ShapeRepetition 等类型
     * @param layer shape 对象所在的 layer
     * @param shape_old 旧数据对象的引用
     * @param shape_new 新数据对象的左或右值引用
     * @return 若成功替换，则返回 true, 否则返回 false
     */
    template <class ShapeType>
    bool ReplaceShape(const Layer &layer, const std::remove_reference_t<ShapeType> &shape_old, ShapeType &&shape_new)
    {
        auto it = shapes_map_.find(layer);
        if (it == shapes_map_.end()) {
            return false;
        }

        if (!it->second.Replace(shape_old, std::forward<ShapeType>(shape_new))) {
            return false;
        }

        using T = std::decay_t<ShapeType>;
        if constexpr (Shapes::IsShapeWithArea(ShapeTraits<T>::enum_value_)) {
            SetDirty(kBoundingBoxSpatialIndexDirty, &layer, 1);
        }
        return true;
    }

    /**
     * @brief 删除指定 shapes 集合的数据。注意，此接口可能导致外部持有的 Shapes 对象的引用失效。
     * 此接口要么删除集合中所有的元素并返回 true，要么都不删除并返回 false
     *
     * @tparam ShapeType BoxI/PolygonI/PathI/Text/ShapeRepetition 等类型
     * @param layer shape 对象所在的 layer
     * @param shapes 待删除对象的指针集合
     * @return 若成功删除，则返回 true, 否则返回 false
     */
    template <class ShapeType>
    bool DeleteShapes(const Layer &layer, const MeSet<ShapeType *> &shapes)
    {
        auto it = shapes_map_.find(layer);
        if (it == shapes_map_.end()) {
            return false;
        }

        if (!it->second.DeleteShapes(shapes)) {
            return false;
        }

        using T = std::decay_t<ShapeType>;
        if (it->second.Size(true) == 0) {
            shapes_map_.erase(it);
            if constexpr (Shapes::IsShapeWithArea(ShapeTraits<T>::enum_value_)) {
                SetDirty(kAllDirty, &layer, 1);
            } else {
                SetDirty(kLayerDirty, &layer, 1);
            }
            return true;
        }

        if constexpr (Shapes::IsShapeWithArea(ShapeTraits<T>::enum_value_)) {
            SetDirty(kBoundingBoxSpatialIndexDirty, &layer, 1);
        }
        return true;
    }

    /**
     * @brief 移动或拷贝(追加)其他 Cell 的所有图形以及 Instances 到当前 Cell, 可能修改子 Cell 的 parent_cells_,
     * 线程不安全
     *
     * @param cell 需要被拷贝或者被移动的 Cell，可左值可右值
     */
    template <class CellType>
    void CopyFromCell(CellType &&cell);

    /**
     * @brief 当前 Cell 新增一个 Instance, 会修改子 Cell 的 parent_cells_, 线程不安全
     *
     * @param inst 需要新增的 Instance, 可左值可右值
     */
    template <class InstType>
    void InsertInstance(InstType &&inst);

    /**
     * @brief 删除指定的Instance, 若有重复, 则全删除, 会修改子 Cell 的 parent_cells_, 线程不安全
     *
     * @param cell 需要删除的 Instance 所指向 Cell 的指针
     * @return 若成功删除，则返回 true, 否则返回 false
     */
    bool DeleteInstances(Cell &cell)
    {
        auto new_end = std::remove_if(instances_.begin(), instances_.end(),
                                      [&cell](const Instance &inst) { return inst.CellPtr() == &cell; });
        if (new_end == instances_.end()) {
            return false;
        }

        instances_.erase(new_end, instances_.end());
        cell.parent_cells_.erase(this);

        SetDirty();
        ClearMaxLevel();
        return true;
    }

    /**
     * @brief 删除指定 instances 集合的数据。注意，此接口可能导致外部持有的 Instance 对象的引用失效。
     * 此接口要么删除集合中所有的元素并返回 true，要么都不删除并返回 false，线程不安全
     *
     * @param instances 待删除对象的指针集合
     * @return 若成功删除，则返回 true, 否则返回 false
     */
    bool DeleteInstances(const MeSet<Instance *> &instances)
    {
        if (instances.empty() || instances_.empty()) {
            return false;
        }

        if (std::any_of(instances.begin(), instances.end(), [this](const auto &instance) {
                return instance < &instances_.front() || instance > &instances_.back();
            })) {
            return false;
        }

        MeUnorderedSet<Cell *> deleted_cells;
        for (auto it = instances.rbegin(); it != instances.rend(); it++) {
            deleted_cells.emplace((*it)->CellPtr());
            int64_t index = std::distance(instances_.data(), *it);
            if (index != static_cast<int64_t>(instances_.size()) - 1) {
                instances_[index] = std::move(instances_.back());
            }
            instances_.pop_back();
        }

        // 查询Cell父子关系，并清理
        for (const auto &instance : instances_) {
            auto it = deleted_cells.find(instance.CellPtr());
            if (it != deleted_cells.end()) {  // 还存在，不需要消除父子关系
                deleted_cells.erase(it);
                if (deleted_cells.empty()) {
                    break;
                }
            }
        }

        for (auto &cell : deleted_cells) {
            cell->ParentCells().erase(this);
        }

        SetDirty();
        ClearMaxLevel();
        return true;
    }

    /**
     * @brief 清空所有的 Instances, 会修改子 Cell 的 parent_cells_, 线程不安全
     */
    void ClearInstances()
    {
        for (const auto &inst : instances_) {
            inst.CellPtr()->parent_cells_.erase(this);
        }
        instances_.clear();
        SetDirty();
        ClearMaxLevel();
    }

    /**
     * @brief 获取所有子 Cell（不包含孙 Cell）
     *
     * @return MeVector<Instance>& 所有的子 Cell，不递归包含孙 Cell
     */
    const MeVector<Instance> &Instances() const { return instances_; }

    /**
     * @brief 获取所有子 Cell（不包含孙 Cell）用于修改，修改完成前不要调用 GetBoundingBox 接口
     *
     * @return MeVector<Instance>& 所有的子 Cell，不递归包含孙 Cell
     */
    MeVector<Instance> &InstancesForWrite()
    {
        SetDirty();
        ClearMaxLevel();
        return instances_;
    }

    /**
     * @brief 获取 Cell 的名称
     *
     * @return const std::string& Cell 的名称
     */
    const std::string &Name() const { return name_; }

    /**
     * @brief 判断 Layer 是否存在 Cell 内, 不含子孙 cell
     *
     * @param layer 需要判断的 Layer
     * @return 存在返回 true，不存在返回 false
     */
    bool HasLayer(const Layer &layer) const { return shapes_map_.find(layer) != shapes_map_.end(); }

    /**
     * @brief 判断 Layer 是否存在 Cell 内，含子孙 cell
     *
     * @param layer 需要判断的 Layer
     * @return true 存在
     * @return false 不存在
     */
    bool HasLayerIncludeChildren(const Layer &layer) const
    {
        if (!layer_dirty_.LayerFlag()) {
            return std::binary_search(layer_cache_.begin(), layer_cache_.end(), layer);
        }

        if (HasLayer(layer)) {
            return true;
        }

        for (auto const &inst : instances_) {
            const auto *cell = inst.CellPtr();
            if (cell != nullptr && cell->HasLayerIncludeChildren(layer)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 获取指定 Layer 上的边界框，含子孙 cell
     *
     * @param layer 要获取边界框的层
     * @return BoxI 边界框
     */
    BoxI GetBoundingBox(const Layer &layer) const
    {
        // 无此 layer， 则直接返回
        if (!HasLayerIncludeChildren(layer)) {
            return {};
        }

        return GetOrCalculateBoundingBox(layer);
    }

    /**
     * @brief 获取整个 Cell 的边界框，含子孙 cell
     *
     * @return BoxI 整个 Cell 的边界框
     */
    BoxI GetBoundingBox() const
    {
        BoxI ret;
        auto const layers = LayersIncludeChildren();
        for (auto const &layer : layers) {
            BoxUnion(ret, GetOrCalculateBoundingBox(layer));
        }
        return ret;
    }

    /**
     * @brief 递归获取获取当前 cell 的所有子孙 Cell。
     *
     * @return 子孙 cells, 无重复元素，无序
     */
    std::vector<Cell *> GetDescendantCells() const;

    /**
     * @brief 获取指定区域中的子孙 cells
     *
     * @param region 指定的区域
     * @param level 搜索的层级深度
     * @return 子孙 cells，无重复元素，无序
     */
    std::vector<Cell *> GetDescendantCells(const BoxI &region, uint32_t level) const;

    /**
     * @brief 拷贝一个 src 上的 shapes 到 dst 上。
     *
     * @param src 被拷贝的 layer, 必须存在，否则不会发生拷贝
     * @param dst 拷贝的目标 layer
     */
    void CopyLayer(const Layer &src, const Layer &dst)
    {
        if (shapes_map_.find(src) != shapes_map_.end()) {
            shapes_map_[dst].Merge(shapes_map_[src]);
            SetDirty(kAllDirty, &dst, 1);
        }
    }

    /**
     * @brief 移动 src 上的 shapes 到 dst 上。src 需要存在，dst 不存在。移动后 src 将不存在。
     *
     * @param src 被移动的 layer，移动完成后将被删除；必须存在，否则不会发生移动
     * @param dst 目标 layer，在当前 Cell 中必须不存在，否则不会发生移动
     */
    void MoveLayer(const Layer &src, const Layer &dst)
    {
        if (shapes_map_.find(src) != shapes_map_.end() && shapes_map_.find(dst) == shapes_map_.end()) {
            shapes_map_.emplace(dst, std::move(shapes_map_[src]));
            shapes_map_.erase(src);
            MeVector<Layer> layers{src, dst};
            SetDirty(kAllDirty, layers.data(), layers.size());
        }
    }

    /**
     * @brief 删除当前 Cell 中指定的 layer
     *
     * @param layer 被删除的 layer
     */
    void Remove(const Layer &layer)
    {
        if (shapes_map_.erase(layer) == 1) {
            SetDirty(kAllDirty, &layer, 1);
        }
    }

    /**
     * @brief 更新 Cell 的缓存数据，可更新的内容请参考 flag 字段。由用户主动调用
     *
     * @param flag 其含义请参考 const.h 中的 kUpdateXXX 变量，当前仅支持 kUpdateSpatialIndex 更新空间索引
     * @param option 用于更新操作的选项，
     */
    void Update(uint16_t flag, const CellUpdateOption &option)
    {
        if ((flag & kUpdateSpatialIndex) != 0) {
            const auto &layers = option.Layers().empty() ? LayersIncludeChildren() : option.Layers();
            CreateSpatialIndex(option.UpdatedSpatialIndexOption(), layers, option.IsEmptySpatialIndex());
        }
    }

    /**
     * @brief 查询指定的 layer 是否已创建了有效的空间索引
     *
     * @param layer 被查询的 layer
     * @return 指定的 layer 上是否创建了有效的空间索引
     */
    bool HasSpatialIndex(const Layer &layer) const
    {
        auto it = cache_.find(layer);
        return it != cache_.end() && !it->second.SpatialIndexDirty();
    }

    /**
     * @brief 对当前 Cell 进行变换操作
     *
     * @param trans 需要做的变换,包含1.镜像；2.缩放；3.旋转；4.位移
     * @return 改变当前 Cell 数据,返回自身
     */
    template <class TransType>
    Cell &Transform(const TransType &trans)
    {
        for (auto &[_, shapes] : shapes_map_) {
            shapes.Transform(trans);
        }

        for (auto &inst : instances_) {
            inst.TransformPlacement(trans);
        }
        const auto &layers = LayersIncludeChildren();
        SetDirty(kBoundingBoxSpatialIndexDirty, layers.data(), layers.size());
        return *this;
    }

    /**
     * @brief 对 Cell 进行压缩
     *
     * @param compress_level 压缩级别，控制压缩算法及压缩内容
     */
    void Compress(uint32_t compress_level)
    {
        for (auto &[_, shapes] : shapes_map_) {
            shapes.Compress(compress_level);
        }
        const auto &layers = Layers();
        SetDirty(kSpatialIndexDirty, layers.data(), layers.size());
    }

    /**
     * @brief 对 Cell 进行解压缩
     */
    void Decompress()
    {
        for (auto &[_, shapes] : shapes_map_) {
            shapes.Decompress();
        }
        const auto &layers = Layers();
        SetDirty(kSpatialIndexDirty, layers.data(), layers.size());
    }

    /**
     * @brief 统计指定 layer 上的有面积类型的图形的数量，含子孙 Cell
     *
     * @return 指定的 layer 上的图形数量
     */
    size_t GetPolygonCount(const Layer &layer)
    {
        size_t res = GetShapes(layer).Size(false);
        for (const auto &inst : instances_) {
            res += inst.CellPtr()->GetPolygonCount(layer) * inst.PlacementPtr()->Size();
        }
        return res;
    }

    /**
     * @brief 递归获取当前 Cell 对象的层级结构的最大层级数，并缓存此值。层级数从 0 开始计数
     *
     * @return 最大 Cell 层级数
     */
    uint32_t GetMaxLevel() const
    {
        if (max_level_ != kInvalidMaxLevel) {
            return max_level_;
        }

        max_level_ = 0;
        if (instances_.empty()) {
            return max_level_;
        }

        uint32_t max_level = 0;
        for (auto const &inst : instances_) {
            auto sub_level = inst.CellPtr()->GetMaxLevel();
            if (sub_level > max_level) {
                max_level = sub_level;
            }
        }
        ++max_level;

        if (max_level < kInvalidMaxLevel) {
            max_level_ = static_cast<uint8_t>(max_level);
        } else {
            max_level_ = kInvalidMaxLevel;
        }

        return max_level;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    /**
     * @brief 用以记录某个 Layer 上的 BoundingBox 与 SpatialIndex 的缓存数据
     */
    class CellCache {
    public:
        CellCache() = default;
        explicit CellCache(const BoxI &box) : bbox_(box) { SetBoundingBoxDirty(false); }
        CellCache(const SpatialIndexOption &option, const MeVector<Element> &elements)
            : spatial_index_ptr_(std::make_unique<SpatialIndex>(option, elements))
        {
            SetSpatialIndexDirty(false);
        }
        CellCache(const CellCache &other) { *this = other; }
        CellCache &operator=(const CellCache &other)
        {
            if (&other != this) {
                bbox_ = other.bbox_;
                SetBoundingBoxDirty(other.BoundingBoxDirty());
                spatial_index_ptr_ = nullptr;
                SetSpatialIndexDirty(true);
            }
            return *this;
        }
        CellCache(CellCache &&) = default;
        CellCache &operator=(CellCache &&) = default;

        bool BoundingBoxDirty() const { return dirty_.BoundingBoxFlag(); }
        void SetBoundingBoxDirty(bool dirty) { dirty_.SetBoundingBoxFlag(dirty); }
        const BoxI &BoundingBox() const { return bbox_; }
        void SetBoundingBox(const BoxI &box)
        {
            bbox_ = box;
            SetBoundingBoxDirty(false);
        }

        bool SpatialIndexDirty() const { return dirty_.SpatialIndexFlag(); }
        void SetSpatialIndexDirty(bool dirty) { dirty_.SetSpatialIndexFlag(dirty); }
        const std::unique_ptr<SpatialIndex> &SpatialIndexPtr() const { return spatial_index_ptr_; }
        void SetSpatialIndexPtr(const SpatialIndexOption &option, const MeVector<Element> &elements)
        {
            spatial_index_ptr_ = std::make_unique<SpatialIndex>(option, elements);
            SetSpatialIndexDirty(false);
        }

        USE_MEDB_OPERATOR_NEW_DELETE

    private:
        BoxI bbox_;
        std::unique_ptr<SpatialIndex> spatial_index_ptr_;
        DirtyFlag dirty_;
    };

    /**
     * @brief 设置 Cell 的名称
     *
     * @param name 指定的 Cell 名称
     */
    void SetName(const std::string &name) { name_ = name; }

    /**
     * @brief 若 Cell 的图形数据改变，需要将对应 Layer 的缓存置为 dirty 状态，并标记上层的所有 parents
     *
     * @param flag dirty 标志集，非 dirty 的不会生效
     * @param layers 待设置的 layers 的头指针。使用指针是为了同时支持 1 个和多个 layer，并避免构造临时的 vector<Layer>
     * @param size layers 的 size
     */
    void SetDirty(const DirtyFlag &flag, const Layer *layers, size_t size) const
    {
        bool need_update_parents = false;
        if (flag.LayerFlag() && !layer_dirty_.LayerFlag()) {
            layer_dirty_.SetLayerFlag(true);
            need_update_parents = true;
        }

        for (int i = 0; i < static_cast<int>(size); ++i) {
            auto it = cache_.find(layers[i]);
            if (it == cache_.end()) {
                continue;
            }

            auto &cache = it->second;
            if (flag.BoundingBoxFlag() && !cache.BoundingBoxDirty()) {
                cache.SetBoundingBoxDirty(true);
                need_update_parents = true;
            }

            if (flag.SpatialIndexFlag() && !cache.SpatialIndexDirty()) {
                cache.SetSpatialIndexDirty(true);
                need_update_parents = true;
            }
        }

        if (need_update_parents) {
            for (auto &parent : parent_cells_) {
                parent->SetDirty(flag, layers, size);
            }
        }
    }

    /**
     * @brief 将所有缓存置为 dirty 状态，并标记上层的所有 parents
     */
    void SetDirty() const
    {
        bool need_update_parents = false;
        if (!layer_dirty_.LayerFlag()) {
            layer_dirty_.SetLayerFlag(true);
            need_update_parents = true;
        }

        for (auto &[_, cache] : cache_) {
            if (!cache.BoundingBoxDirty()) {
                need_update_parents = true;
                cache.SetBoundingBoxDirty(true);
            }

            if (!cache.SpatialIndexDirty()) {
                need_update_parents = true;
                cache.SetSpatialIndexDirty(true);
            }
        }

        if (need_update_parents) {
            for (auto &parent : parent_cells_) {
                parent->SetDirty();
            }
        }
    }

    /**
     * @brief 将自身及其所有上级 Cell 的 max_level_ 缓存置为无效值 kInvalidMaxLevel; 仅在 instance 有改变的情况下使用
     */
    void ClearMaxLevel()
    {
        if (max_level_ == kInvalidMaxLevel) {
            return;
        }

        max_level_ = kInvalidMaxLevel;
        for (auto parent : parent_cells_) {
            parent->ClearMaxLevel();
        }
    }

    /**
     * @brief 在当前cell下所有layer，创建六种图形的空间网格索引
     *
     * @param SpatialIndexOption& 生成网格对象的一些配置参数
     * @param std::vector<Layer>& 待创建索引的layer
     */
    void CreateSpatialIndex(const SpatialIndexOption &option, const std::vector<Layer> &layers,
                            bool is_empty_spatial_index)
    {
        if (is_empty_spatial_index) {
            for (auto const &layer : layers) {
                auto iter = cache_.find(layer);
                if (iter == cache_.end()) {
                    cache_.emplace(layer, CellCache());
                }
            }
            return;
        }
        for (auto const &layer : layers) {
            if (!HasLayerIncludeChildren(layer)) {
                continue;
            }
            auto iter = cache_.find(layer);
            // 已存在且不为 dirty 且缓存的 window_step 小于输入的 window_step, 则不重新创建索引
            if (iter != cache_.end() && !(iter->second.SpatialIndexDirty()) &&
                iter->second.SpatialIndexPtr()->Option().WindowStep() < option.WindowStep()) {
                continue;
            }
            MeVector<Element> element_list;
            GetElements(std::make_index_sequence<EToI(ElementType::kShapeNum)>(), layer, element_list);
            for (const auto &inst : instances_) {
                const Element element(static_cast<const void *>(&inst), ElementType::kInstance);
                element_list.emplace_back(element);
            }
            if (element_list.empty()) {
                continue;
            }
            std::optional<Layer> query_layer{layer};
            auto op = option;
            if (op.WindowStep() != 0 && op.Region().Empty()) {
                op.SetRegion(GetBoundingBox(layer));
            }
            op.SetInstanceLayer(query_layer);

            auto [row, col] = SpatialIndex::CalculcateRowColumn(op, element_list);
            if (row == col && row == 1) {
                // 当只有一个格子时，无需创建空间索引
                continue;
            }
            if (iter == cache_.end()) {
                cache_.emplace(std::piecewise_construct, std::forward_as_tuple(layer),
                               std::forward_as_tuple(op, element_list));
            } else {
                iter->second.SetSpatialIndexPtr(op, element_list);
            }
        }
    }

    // 调用此函数时， 需确保 layer 一定是存在的(自身或其后代)
    BoxI GetOrCalculateBoundingBox(const Layer &layer) const
    {
        auto it = cache_.find(layer);
        if (it != cache_.end() && !it->second.BoundingBoxDirty()) {
            return it->second.BoundingBox();
        }

        auto ret = GetShapes(layer).BoundingBox();
        for (auto const &inst : instances_) {
            auto inst_bbox = inst.PlacementPtr()->BoundingBox(inst.CellPtr()->GetBoundingBox(layer));
            BoxUnion(ret, inst_bbox);
        }

        if (it != cache_.end()) {
            it->second.SetBoundingBox(ret);
        } else {
            cache_.emplace(layer, ret);
        }

        return ret;
    }

    template <size_t... N>
    void GetElements(std::index_sequence<N...>, const Layer &layer, MeVector<Element> &element_list)
    {
        (GetElements<N>(layer, element_list), ...);
    }

    template <size_t kElementType>
    void GetElements(const Layer &layer, MeVector<Element> &element_list)
    {
        using ShapeType = typename ShapeIndexTraits<kElementType>::Type::ShapeType;
        auto shapes_iter = shapes_map_.find(layer);
        if (shapes_iter == shapes_map_.end()) {
            return;
        }
        if constexpr (!Shapes::IsShapeWithArea(static_cast<ElementType>(kElementType))) {
            return;
        }
        // 先对 Repetition 类型的图形进行排序，提高空间索引的创建和查询效率
        if constexpr (Shapes::IsRepType(static_cast<ElementType>(kElementType))) {
            shapes_iter->second.SortRepetition<ShapeType>();
        }

        if (shapes_iter->second.GetShapeTable(static_cast<ElementType>(kElementType)) != nullptr) {
            const auto &shapes = shapes_iter->second.Raw<ShapeType>();
            for (const auto &s : shapes) {
                const Element element(&s, static_cast<ElementType>(kElementType));
                element_list.emplace_back(element);
            }
        }
    }

    /**
     * @brief 为并行导入版图使用，Cell 的 Instance 移动到另一个 Layout 的 Cell 上时，需要更新 Instance 所指向的 Cell
     * 指针, 以及 parent
     *
     * @param cell_name_map Layout 中缓存 Cell 指针和名字的容器
     */
    void ResetCellForInternalUse(const MeUnorderedMap<std::string, MeList<Cell>::iterator> &cell_name_map)
    {
        for (auto it = parent_cells_.begin(); it != parent_cells_.end();) {
            auto cell_name_it = cell_name_map.find((*it)->Name());
            if (cell_name_it != cell_name_map.end() && *it != &*(cell_name_it->second)) {
                it = parent_cells_.erase(it);
                // 若存在重复的父 Cell，第二次 insert 不会成功
                parent_cells_.insert(&*(cell_name_it->second));
            } else {
                ++it;
            }
        }

        for (auto it = instances_.begin(); it != instances_.end(); ++it) {
            auto cell_name_it = cell_name_map.find(it->CellPtr()->Name());
            if (cell_name_it != cell_name_map.end()) {
                it->SetCellPtr(&*cell_name_it->second);
            }
        }
        SetDirty();
    }

    /**
     * @brief 为并行导入版图使用，把其他和当前 Layout 的 Cell 同名的 Cell 移动到当前 Cell，包括 Shapes，instances 以及
     * parents
     *
     * @param cell 需要移动的 Cell 的引用
     */
    void MoveCellForInternalUse(Cell &cell)
    {
        for (auto &layer : cell.Layers()) {
            shapes_map_[layer].Merge(std::move(cell.shapes_map_[layer]));
        }

        if (parent_cells_.empty()) {
            parent_cells_ = std::move(cell.parent_cells_);
        } else {
            parent_cells_.insert(std::make_move_iterator(cell.parent_cells_.begin()),
                                 std::make_move_iterator(cell.parent_cells_.end()));
            cell.parent_cells_.clear();
        }

        if (instances_.empty()) {
            instances_ = std::move(cell.instances_);
        } else {
            instances_.insert(instances_.end(), std::make_move_iterator(cell.instances_.begin()),
                              std::make_move_iterator(cell.instances_.end()));
            cell.instances_.clear();
        }
    }

private:
    ShapesMap shapes_map_;  // 当前 cell 自身的 shapes
    MeVector<Instance> instances_;
    std::string name_;
    MeUnorderedSet<Cell *> parent_cells_;
    mutable MeVector<Layer> layer_cache_;  // 有序的 layers 缓存
    mutable MeMap<Layer, CellCache> cache_;
    mutable DirtyFlag layer_dirty_;
    mutable uint8_t max_level_{kInvalidMaxLevel};

    friend class Layout;
    friend class FlattenTaskInfo;
    friend class CellElementIterator;
};

}  // namespace medb2

#endif  // MEDB_CELL_H
