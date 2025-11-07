/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_SHAPES_H
#define MEDB_SHAPES_H

#include <array>
#include <utility>
#include <vector>
#include "medb/allocator.h"
#include "medb/box.h"
#include "medb/enums.h"
#include "medb/path.h"
#include "medb/polygon.h"
#include "medb/repetition_utils.h"
#include "medb/text.h"
#include "medb/traits.h"
#include "polygon_utils.h"

namespace medb2 {

/**
 * @brief 存放版图文件内Box、Path、Text、Path的容器
 *
 */
class Shapes {
public:
    static constexpr std::array<ElementType, 5> kShapesWithArea{
        ElementType::kBox, ElementType::kPolygon, ElementType::kPath, ElementType::kBoxRep, ElementType::kPolygonRep};

    /**
     * @brief 判断传入的 ElementType 是否为 ShapeRepetition 类型
     *
     */
    static constexpr bool IsRepType(ElementType shape_type)
    {
        for (auto &type : kRepetitionTypes) {
            if (shape_type == type) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 判断传入的 ElementType 是否为有面积的图形，Text 没有面积
     *
     */
    static constexpr bool IsShapeWithArea(ElementType shape_type)
    {
        for (auto &type : kShapesWithArea) {
            if (shape_type == type) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 便于将同一个函数应用于不同的图形容器
     *
     * @param f 函数指针，必须是模板函数，且模板参数为 size_t（表示第 N 个图形种类）
     * @param args 函数的参数
     */
    template <class F, class... Args>
    static void LoopForShape(F &&f, Args &&...args)
    {
        LoopForShape(std::make_index_sequence<EToI(ElementType::kShapeNum)>(), std::forward<F &&>(f),
                     std::forward<Args>(args)...);
    }

public:
    // 默认构造函数
    Shapes() = default;

    // 析构函数
    ~Shapes() { Clear(); }

    /**
     * @brief 复制构造
     *
     * @param s 被复制Shapes的对象引用
     */
    Shapes(const Shapes &s) { *this = s; }

    /**
     * @brief 移动构造
     *
     * @param s 被移动Shapes的对象右值引用
     */
    Shapes(Shapes &&s) { *this = std::move(s); }

    /**
     * @brief 复制赋值等号运算符
     *
     * @param s 被复制Shapes的对象引用
     * @return Shapes& 自身对象引用
     */
    Shapes &operator=(const Shapes &s)
    {
        if (&s != this) {
            Assign(std::make_index_sequence<EToI(ElementType::kShapeNum)>(), s);
        }
        return *this;
    }

    /**
     * @brief 移动赋值等号运算符
     *
     * @param s 被移动Shapes的对象右值引用
     * @return Shapes& 自身对象引用
     */
    Shapes &operator=(Shapes &&s)
    {
        if (&s != this) {
            Clear();
            std::swap(shape_table_, s.shape_table_);
            std::swap(bounding_box_, s.bounding_box_);
            std::swap(dirty_, s.dirty_);
        }
        return *this;
    }

    /**
     * @brief 对当前 shapes 进行变换操作
     *
     * @param trans 需要做的变换,包含1.镜像；2.缩放；3.旋转；4.位移
     * @return 改变当前 shapes 数据,返回自身
     */
    template <class TransType>
    Shapes &Transform(const TransType &trans)
    {
        TransformShapes(std::make_index_sequence<EToI(ElementType::kShapeNum)>(), trans);
        ClearCache();
        return *this;
    }

    /**
     * @brief 对当前 Shapes 进行复制,对副本进行变换操作
     *
     * @param trans 需要做的变换,包含1.镜像；2.缩放；3.旋转；4.位移
     * @return 改变副本 Shapes 数据,返回副本变换后的 Shapes
     */
    template <class TransType>
    Shapes Transformed(const TransType &trans) const
    {
        Shapes shapes_ret = *this;
        shapes_ret.Transform(trans);
        return shapes_ret;
    }

    /**
     * @brief 给对应 vector 容器进行扩容
     *
     * @param size 扩容后的大小
     */
    template <class ShapeTypes>
    void Reserve(size_t size)
    {
        RawData<ShapeTypes>().reserve(size);
    }

    /**
     * @brief 移动，根据对应的 ShapeTypes 的类型获取到 MeVector 容器，把内容移动到当前的容器中
     *
     * @param s 待移动的 MeVector 数据容器
     */
    template <class ShapeTypes>
    void InsertShapes(MeVector<ShapeTypes> &&s)
    {
        if (s.empty()) {
            return;
        }
        std::move(s.begin(), s.end(), std::back_inserter(RawData<ShapeTypes>()));
        ClearCache();
    }

    /**
     * @brief 拷贝，根据对应的 ShapeTypes 的类型获取到 MeVector 容器，把内容拷贝到当前的容器中
     *
     * @param s 待拷贝的 MeVector 数据容器
     */
    template <class ShapeTypes>
    void InsertShapes(const MeVector<ShapeTypes> &s)
    {
        if (s.empty()) {
            return;
        }
        std::copy(s.begin(), s.end(), std::back_inserter(RawData<ShapeTypes>()));
        ClearCache();
    }

    /**
     * @brief 把其他的 Shapes 插入到当前 Shapes 中，包括Box，Polygon，Text，Path，ShapeRepetition<BoxI> 和
     * ShapeRepetition<PolygonI>
     *
     * @param s 复制的 Shapes 对象, 可左值可右值, 左值拷贝后原 Shapes 数据存在, 右值移动后原 Shapes 数据不存在
     */
    template <typename Shapes>
    void Merge(Shapes &&s)
    {
        if (s.Size(true) == 0) {
            return;
        }
        MergeData(std::make_index_sequence<EToI(ElementType::kShapeNum)>(), std::forward<Shapes>(s));
        ClearCache();
    }

    /**
     * @brief 把 Box，Polygon，Text 或 Path 插入到当前 Shapes 中
     *        使用了perfect forwarding基础，shape可以是引用、常量引用、右值
     *
     * @param shape 插入的数据，包括Box，Polygon，Text，Path，ShapeRepetition<BoxI> 和 ShapeRepetition<PolygonI>
     * @return 返回已经插入到容器中的 shape 的引用
     */
    template <class ShapeType>
    auto &Insert(ShapeType &&shape)
    {
        auto &shapes = RawData<std::remove_const_t<std::remove_reference_t<ShapeType>>>();
        auto &shape_ref = shapes.emplace_back(std::forward<ShapeType>(shape));
        ClearCache();
        return shape_ref;
    }

    /**
     * @brief 替换指定 shape 的数据，用于对某个 shape 的修改操作。此操作不会修改原对象的内存地址
     *
     * @tparam ShapeType BoxI/PolygonI/PathI/Text/ShapeRepetition 等类型
     * @param shape_old 旧数据对象的引用
     * @param shape_new 新数据对象的左或右值引用
     * @return 若成功替换，则返回 true, 否则返回 false
     */
    template <class ShapeType>
    bool Replace(const std::remove_reference_t<ShapeType> &shape_old, ShapeType &&shape_new)
    {
        using T = std::decay_t<ShapeType>;
        constexpr ElementType enum_value = ShapeTraits<T>::enum_value_;
        auto shapes = static_cast<MeVector<T> *>(shape_table_[EToI(enum_value)]);
        if (shapes == nullptr || shapes->empty() || &shape_old < &shapes->front() || &shape_old > &shapes->back()) {
            return false;
        }

        auto index = std::distance(static_cast<const T *>(shapes->data()), &shape_old);
        (*shapes)[index] = std::forward<ShapeType>(shape_new);

        if constexpr (IsShapeWithArea(enum_value)) {
            ClearCache();
        }
        return true;
    }

    /**
     * @brief 删除指定 shapes 集合中的数据。此接口要么删除集合中所有的元素并返回 true，要么都不删除并返回 false
     *
     * @tparam ShapeType BoxI/PolygonI/PathI/Text/ShapeRepetition 等类型
     * @param shapes_deleted 待删除对象的指针集合
     * @return 若成功删除，则返回 true, 否则返回 false
     */
    template <class ShapeType>
    bool DeleteShapes(const MeSet<ShapeType *> &shapes_deleted)
    {
        using T = std::decay_t<ShapeType>;
        constexpr ElementType enum_value = ShapeTraits<T>::enum_value_;
        auto shapes = static_cast<MeVector<T> *>(shape_table_[EToI(enum_value)]);
        if (shapes == nullptr || shapes->empty() || shapes_deleted.empty()) {
            return false;
        }

        if (std::any_of(shapes_deleted.begin(), shapes_deleted.end(),
                        [&shapes](const auto &s) { return s < &shapes->front() || s > &shapes->back(); })) {
            return false;
        }

        for (auto it = shapes_deleted.rbegin(); it != shapes_deleted.rend(); ++it) {
            const ShapeType *shape = *it;
            int64_t index = std::distance(std::as_const(*shapes).data(), shape);
            if (index != (static_cast<int64_t>(shapes->size()) - 1)) {
                (*shapes)[index] = std::move(shapes->back());
            }
            shapes->pop_back();
        }

        if constexpr (IsShapeWithArea(enum_value)) {
            ClearCache();
        }
        return true;
    }

    /**
     * @brief 获取不同类型 Shapes 的
     * vector容器主要有：Box，Polygon，Text，Path，ShapeRepetition<BoxI>，ShapeRepetition<PolygonI>; const
     * 类型接口，使用 const shape 对象调用
     *
     * @tparam TransType 参数的类型
     * @tparam ShapeTypes
     * @return MeVector<ShapeTypes>& 对应类型 Shapes 的 vector 容器的引用，若不存在返回空的 vector
     */
    template <class ShapeTypes>
    const MeVector<ShapeTypes> &Raw() const
    {
        constexpr uint8_t enum_value = EToI(ShapeTraits<ShapeTypes>::enum_value_);
        const auto *shape = static_cast<MeVector<ShapeTypes> *>(shape_table_[enum_value]);
        if (!shape) {
            static MeVector<ShapeTypes> empty_vector;
            return empty_vector;
        }
        return *shape;
    }

    /**
     * @brief 通过 shapetype 的类型获取对应类型的 shape_table_ 的数组指针
     *
     * @param shapetype 的六种不同类型
     * @return const void* 数组的 void 指针
     */
    const void *GetShapeTable(const medb2::ElementType &shapetype) const { return shape_table_.at(EToI(shapetype)); }

    /**
     * @brief 对 Shapes 进行压缩, 不改变存储图形的几何位置，仅改变它们的存储方式
     *
     * @param compress_level 压缩级别，控制压缩算法及压缩内容
     */
    void Compress(uint32_t compress_level)
    {
        if (BoxCompressAlgoType(compress_level) != kNoneCompressAlgo) {
            CompressBox(BoxCompressAlgoType(compress_level));
        }
        if (PolygonCompressAlgoType(compress_level) != kNoneCompressAlgo) {
            CompressPolygon(PolygonCompressAlgoType(compress_level));
        }
    }

    /**
     * @brief 对 Shapes 进行解压缩, 不改变存储图形的几何位置，仅改变它们的存储方式
     */
    void Decompress() { Decompress(std::make_index_sequence<EToI(ElementType::kShapeNum)>()); }

    /**
     * @brief 获取 Shapes 对象的 bounding box。注意，不包含 Text 的位置信息
     *
     * @return BoxI Shapes 的 bounding box
     */
    const BoxI &BoundingBox() const
    {
        if (dirty_.BoundingBoxFlag()) {
            bounding_box_.Clear();
            BoundingBoxImpl(std::make_index_sequence<EToI(ElementType::kShapeNum)>());
            dirty_.SetBoundingBoxFlag(false);
        }
        return bounding_box_;
    }

    /**
     * @brief 统计除 Text 外的所有图形数量
     *
     * @param include_shape_without_area 是否包含无面积类型的图形，当前只有 Text 属于无面积类型
     * @return size_t 除 Text 外的所有图形数量
     */
    size_t Size(bool include_shape_without_area) const
    {
        return Size(std::make_index_sequence<EToI(ElementType::kShapeNum)>(), include_shape_without_area);
    }

    /**
     * @brief 清空对象中的数据
     *
     */
    void Clear() { Clear(std::make_index_sequence<EToI(ElementType::kShapeNum)>()); }

    /**
     * @brief 修改Text的名字
     *
     */
    void RenameText(const std::string &old_name, const std::string &new_name)
    {
        if (GetShapeTable(ElementType::kText) == nullptr) {
            return;
        }

        auto &texts = RawData<Text>();
        for (auto &text : texts) {
            if (text.String() == old_name) {
                text.SetString(new_name);
            }
        }
    }

    /**
     * @brief 对shapes中的某种ShapeRepetion类型的图元，进行图元内部排序,以加速reption的bbox的计算
     *
     * @tparam ShapeTypes 必须是仅支持ShapeRepetion类型的图元，当前仅支持BoxRepetion和PolygonRepetition
     */
    template <class ShapeTypes>
    void SortRepetition()
    {
        constexpr ElementType element_type = ShapeTraits<ShapeTypes>::enum_value_;
        if constexpr (IsRepType(element_type)) {
            constexpr uint8_t enum_value = EToI(element_type);
            auto shape_vec_ptr = static_cast<MeVector<ShapeTypes> *>(shape_table_[enum_value]);
            if (shape_vec_ptr) {
                for (auto &shape : *shape_vec_ptr) {
                    shape.Sort();
                }
            }
        }
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    template <size_t... N, class F, class... Args>
    static void LoopForShape(std::index_sequence<N...>, F &&f, Args &&...args)
    {
        (f.template operator()<N>(std::forward<Args>(args)...), ...);
    }

    template <size_t... N, class TransType>
    void TransformShapes(std::index_sequence<N...>, const TransType &trans)
    {
        (TransformShape<N>(trans), ...);
    }

    template <size_t kShapeType, class TransType>
    void TransformShape(const TransType &trans)
    {
        if (shape_table_[kShapeType] != nullptr) {
            auto &shape = RawData<typename ShapeIndexTraits<kShapeType>::Type::ShapeType>();
            for (auto &s : shape) {
                s.Transform(trans);
            }
        }
    }

    template <size_t... N>
    void Assign(std::index_sequence<N...>, const Shapes &s)
    {
        (AssignShapes<N>(s), ...);        // 1. shape table
        bounding_box_ = s.bounding_box_;  // 2. bounding box
        dirty_ = s.dirty_;                // 3. dirty flag
    }

    template <size_t kShapeType>
    void AssignShapes(const Shapes &s)
    {
        auto shapes = s.GetShapes<kShapeType>();
        if (shapes) {
            if (!shape_table_[kShapeType]) {
                shape_table_[kShapeType] = medb_new<typename ShapeIndexTraits<kShapeType>::Type::VectorType>();
            }
            *GetShapes<kShapeType>() = *shapes;
        }
    }

    template <size_t... N, typename Shapes>
    void MergeData(std::index_sequence<N...>, Shapes &&s)
    {
        (MergeData<N>(std::forward<Shapes>(s)), ...);
    }

    template <size_t kShapeType>
    void MergeData(Shapes &&s)
    {
        auto &src = s.RawData<typename ShapeIndexTraits<kShapeType>::Type::ShapeType>();
        if (src.empty()) {
            return;
        }
        auto &dest = RawData<typename ShapeIndexTraits<kShapeType>::Type::ShapeType>();
        if (dest.empty()) {
            dest = std::move(src);
        } else {
            dest.insert(dest.end(), std::make_move_iterator(src.begin()), std::make_move_iterator(src.end()));
            src.clear();
        }
    }

    template <size_t kShapeType>
    void MergeData(const Shapes &s)
    {
        auto &src = s.Raw<typename ShapeIndexTraits<kShapeType>::Type::ShapeType>();
        if (src.empty()) {
            return;
        }
        auto &dest = RawData<typename ShapeIndexTraits<kShapeType>::Type::ShapeType>();
        dest.insert(dest.end(), src.begin(), src.end());
    }

    template <size_t... N>
    void BoundingBoxImpl(std::index_sequence<N...>) const
    {
        (BoundingBoxImpl<N>(), ...);
    }

    template <size_t kShapeType>
    void BoundingBoxImpl() const
    {
        if constexpr (IsShapeWithArea(static_cast<ElementType>(kShapeType))) {
            if (GetShapes<kShapeType>() != nullptr && !GetShapes<kShapeType>()->empty()) {
                BoxUnion(bounding_box_,
                         GetBoundingBox(GetShapes<kShapeType>()->begin(), GetShapes<kShapeType>()->end()));
            }
        }
    }

    template <size_t... N>
    void Clear(std::index_sequence<N...>)
    {
        (ClearShapes<N>(), ...);
        ClearCache();
    }

    void ClearCache() { dirty_.SetBoundingBoxFlag(true); }

    template <size_t kShapeType>
    void ClearShapes()
    {
        if (GetShapes<kShapeType>() != nullptr) {
            medb_delete(GetShapes<kShapeType>());  // 释放资源
            shape_table_[kShapeType] = nullptr;    // 置空
        }
    }

    template <size_t... N>
    size_t Size(std::index_sequence<N...>, bool include_shape_without_area) const
    {
        return (Size<N>(include_shape_without_area) + ...);
    }

    template <size_t kShapeType>
    size_t Size(bool include_shape_without_area) const
    {
        if constexpr (!IsShapeWithArea(static_cast<ElementType>(kShapeType))) {
            if (!include_shape_without_area) {
                return 0ul;
            }
        }
        if (GetShapes<kShapeType>() == nullptr) {
            return 0ul;
        }
        if constexpr (IsRepType(static_cast<ElementType>(kShapeType))) {
            size_t result = 0ul;
            auto &shape_vec = *(GetShapes<kShapeType>());
            for (auto &shape_rep : shape_vec) {
                result += shape_rep.Size();
            }
            return result;
        } else {
            return GetShapes<kShapeType>()->size();
        }
    }

    template <size_t... N>
    void Decompress(std::index_sequence<N...>)
    {
        (DecompressShape<N>(), ...);
    }

    template <size_t kShapeType>
    void DecompressShape()
    {
        if constexpr (IsRepType(static_cast<ElementType>(kShapeType))) {
            if (GetShapes<kShapeType>() == nullptr) {
                return;
            }
            using InnerShapeType = typename ShapeIndexTraits<kShapeType>::Type::ShapeType::InnerShapeType;
            auto &shape_vec = *(GetShapes<kShapeType>());
            auto &inner_shape_vec = RawData<InnerShapeType>();
            inner_shape_vec.reserve(inner_shape_vec.size() + Size<kShapeType>(false));
            for (auto &shape_rep : shape_vec) {
                shape_rep.GetAllShape(inner_shape_vec);
            }
            ClearShapes<kShapeType>();
        }
    }

    void CompressBox(CompressAlgoType algo_type)
    {
        if (GetShapes<EToI(ElementType::kBox)>() == nullptr) {
            return;
        }
        auto &box_vec = *(GetShapes<EToI(ElementType::kBox)>());
        MeUnorderedMap<VectorI, Compressor> box_map;
        for (auto &box : box_vec) {
            VectorI diag_vector = box.TopRight() - box.BottomLeft();
            box_map[diag_vector].AddVector(box.BottomLeft());
        }
        ClearShapes<EToI(ElementType::kBox)>();
        for (auto &[diag_vector, compressor] : box_map) {
            // 如果只有一个图形，就不调用压缩函数了
            if (compressor.Vectors().size() == 1) {
                const auto &offsets = compressor.Vectors();
                RawData<BoxI>().emplace_back(offsets[0], offsets[0] + diag_vector);
                continue;
            }
            MeVector<std::pair<VectorI, Repetition>> shape_reps = compressor.Compress(algo_type);
            for (const auto &[shape_offset, rep] : shape_reps) {
                RawData<ShapeRepetition<BoxI>>().emplace_back(BoxI{shape_offset, shape_offset + diag_vector}, rep);
            }
            const auto &shape_offsets = compressor.Vectors();  // 压缩后剩余的偏移向量
            for (const auto &shape_offset : shape_offsets) {
                RawData<BoxI>().emplace_back(shape_offset, shape_offset + diag_vector);
            }
        }
    }

    void CompressPolygon(CompressAlgoType algo_type)
    {
        if (GetShapes<EToI(ElementType::kPolygon)>() == nullptr) {
            return;
        }
        auto &poly_vec = *(GetShapes<EToI(ElementType::kPolygon)>());
        MeUnorderedMap<PolygonI, Compressor> poly_map;
        for (auto &poly : poly_vec) {
            if (poly.Empty()) {
                continue;
            }
            VectorI offset = poly.PointData()[0];
            poly_map[poly.Transformed(SimpleTransformation(-offset))].AddVector(offset);
        }
        ClearShapes<EToI(ElementType::kPolygon)>();
        for (auto &[base_poly, compressor] : poly_map) {
            // 如果只有一个图形，就不调用压缩函数了
            if (compressor.Vectors().size() == 1) {
                const auto &offsets = compressor.Vectors();
                // 若 poly 存在 BoundingBox 缓存，则其进行 SimpleTransform 后的 base_poly 也存在 BoundingBox
                // 缓存，不需要重新计算
                RawData<PolygonI>().emplace_back(base_poly.Transformed(SimpleTransformation(offsets[0])));
                continue;
            }
            MeVector<std::pair<VectorI, Repetition>> shape_reps = compressor.Compress(algo_type);
            for (const auto &[shape_offset, rep] : shape_reps) {
                RawData<ShapeRepetition<PolygonI>>().emplace_back(
                    base_poly.Transformed(SimpleTransformation(shape_offset)), rep);
            }
            const auto &shape_offsets = compressor.Vectors();  // 压缩后剩余的偏移向量
            for (const auto &shape_offset : shape_offsets) {
                RawData<PolygonI>().emplace_back(base_poly.Transformed(SimpleTransformation(shape_offset)));
            }
        }
    }

    /**
     * @brief 获取不同类型 Shapes 的 vector容器主要有：Box，Polygon，Text，Path
     *
     * @tparam TransType 参数的类型
     * @tparam ShapeTypes
     * @return MeVector<ShapeTypes>& 对应类型 Shapes 的vector 容器
     */
    template <class ShapeTypes>
    MeVector<ShapeTypes> &RawData()
    {
        constexpr uint8_t enum_value = EToI(ShapeTraits<ShapeTypes>::enum_value_);
        auto shape = static_cast<MeVector<ShapeTypes> *>(shape_table_[enum_value]);
        if (!shape) {
            shape_table_[enum_value] = medb_new<MeVector<ShapeTypes>>();
            return *static_cast<MeVector<ShapeTypes> *>(shape_table_[enum_value]);
        }
        return *shape;
    }

    template <size_t kShapeType>
    auto GetShapes() const -> typename ShapeIndexTraits<kShapeType>::Type::VectorType *
    {
        return static_cast<typename ShapeIndexTraits<kShapeType>::Type::VectorType *>(shape_table_[kShapeType]);
    }

private:
    static constexpr std::array<ElementType, 2> kRepetitionTypes{{ElementType::kBoxRep, ElementType::kPolygonRep}};
    // 裸指针是下标ShapeType对应的vector类型，比如第0个元素kBox对应std::vector<Box> *
    std::array<void *, EToI(ElementType::kShapeNum)> shape_table_{};
    mutable BoxI bounding_box_;
    mutable DirtyFlag dirty_;  // 标识缓存数据是否为脏数据

    friend class FlattenTaskInfo;
    friend class Layout;
};

}  // namespace medb2

#endif  // MEDB_SHAPES_H
