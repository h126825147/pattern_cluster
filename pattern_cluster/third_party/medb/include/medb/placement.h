/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_PLACEMENT_H
#define MEDB_PLACEMENT_H

#include <memory>
#include <variant>
#include "medb/allocator.h"
#include "medb/array_info.h"
#include "medb/enums.h"
#include "medb/point.h"
#include "medb/repetition.h"
#include "medb/transformation.h"
#include "medb/transformation_utils.h"

namespace medb2 {
class MEDB_API Placement {
public:
    // 使用仿函数(空类)实现 PlacementDeleter，以支持空基类优化，std::unique_ptr<Placement, Deleter> 的大小为 8 字节
    struct MEDB_API Deleter {
        void operator()(Placement *placement);
    };

    using UniquePtrType = std::unique_ptr<Placement, Deleter>;
    /**
     * @brief 对传入的 Placement 指针进行深拷贝，并返回拷贝后对象的指针
     */
    static UniquePtrType CopyPlacement(const Placement *placement);

public:
    /**
     * @brief 获取 Placement 的总数量
     *
     * @return size_t 数量, 若 placement 的类型为 Single, 返回 1
     */
    size_t Size() const;

    /**
     * @brief 根据传入的基础包围盒，计算该包围盒经过 Placement 的变换后的实际包围盒
     *
     * @param box_base 入参，表示一个 Cell 的基础包围盒
     * @return BoxI 返回 Instance 的实际包围盒
     */
    BoxI BoundingBox(const BoxI &box_base) const;

    /**
     * @brief 对 Placement 进行原地 Transform 变换
     *
     * @param trans Transformation 或 SimpleTransformation 对象
     * @return bool 若当前 Placement 内部的 trans_ 变量能够接收变换后的结果，则返回 true，否则返回 false
     */
    template <class TransType>
    bool Transform(const TransType &trans);

    /**
     * @brief 对 Placement 进行 Transform 变换，并将变换后的结果构造成一个新的 Placement 指针
     *
     * @param trans Transformation 或 SimpleTransformation 对象
     * @return std::unique_ptr<Placement, DeleterType> 变换后的 Placement 指针
     */
    template <class TransType>
    UniquePtrType Transformed(const TransType &trans) const;

    /**
     * @brief 获取第 index 个 trans 变量
     *
     * @return std::variant<SimpleTransformation, Transformation> 返回获取的 trans
     * 变量，若超出范围，则返回对应类型的默认 trans
     */
    TransformationVar Trans(size_t index) const;

    /**
     * @brief 获取 Placement 内部的原始 trans 变量
     *
     * @return std::variant<SimpleTransformation, Transformation> 返回获取的 trans 变量
     */
    TransformationVar RawTrans() const;

    /**
     * @brief 获取 Placement 内部的原始 Repetition 变量
     *
     * @return const Repetition * 若 Placement 内部含有 Repetition 变量，则返回其指针，否则返回 nullptr
     */
    const Repetition *RawRepetition() const;

    /**
     * @brief 将当前 Placement 转为字符串，便于 debug
     *
     * @return std::string Placement 的字符串描述
     */
    std::string ToString() const;

    /**
     * @brief 获取当前 Placement 的类型
     *
     * @return PlacementType 当前 Placement 的类型
     */
    PlacementType Type() const { return placement_type_; }

protected:
    ~Placement() = default;
    Placement() = default;
    explicit Placement(PlacementType type) : placement_type_(type) {};

private:
    const PlacementType placement_type_{kInvalidPlacement};
};

template <class TransType>
class SinglePlacement : public Placement {
    static_assert(std::is_same_v<TransType, SimpleTransformation> || std::is_same_v<TransType, Transformation>,
                  "Just support SimpleTransformation or Transformation");

public:
    SinglePlacement() : Placement(InitPlacementType()) {}

    explicit SinglePlacement(const TransType &trans) : Placement(InitPlacementType()), trans_(trans) {}
    ~SinglePlacement() = default;
    SinglePlacement(const SinglePlacement &other) = default;
    SinglePlacement(SinglePlacement &&other) noexcept = default;
    SinglePlacement &operator=(const SinglePlacement &p) = default;
    SinglePlacement &operator=(SinglePlacement &&p) noexcept = default;

    size_t SizeImpl() const { return 1; }

    BoxI BoundingBoxImpl(const BoxI &box_base) const { return box_base.Transformed(trans_); }

    template <class OtherTransType>
    bool TransformImpl(const OtherTransType &trans)
    {
        if constexpr (std::is_same_v<TransType, SimpleTransformation> &&
                      std::is_same_v<OtherTransType, Transformation>) {
            return false;
        } else {
            trans_ = Compose(trans, trans_);
            return true;
        }
    }

    template <class OtherTransType>
    UniquePtrType TransformedImpl(const OtherTransType &trans) const;

    TransType TransImpl(size_t index) const
    {
        if (index >= SizeImpl()) {
            return TransType();
        }
        return trans_;
    }

    const TransType &RawTransImpl() const { return trans_; }

    std::string ToStringImpl() const
    {
        if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
            return "placement_type: Simple Single Ref\n" + trans_.ToString();
        } else {
            return "placement_type: Single Ref\n" + trans_.ToString();
        }
    }

    void SetTransImpl(const TransType &trans) { trans_ = trans; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    /**
     * @brief 根据模板参数生成对应的 PlacementType
     */
    static constexpr PlacementType InitPlacementType()
    {
        if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
            return kSimpleSRef;
        } else if constexpr (std::is_same_v<TransType, Transformation>) {
            return kSRef;
        } else {
            return kInvalidPlacement;
        }
    }

private:
    TransType trans_;
};

template <class TransType>
class RepPlacement : public Placement {
    static_assert(std::is_same_v<TransType, SimpleTransformation> || std::is_same_v<TransType, Transformation>,
                  "Just support SimpleTransformation or Transformation");

public:
    RepPlacement() : Placement(InitPlacementType()) {}
    explicit RepPlacement(const TransType &trans, const Repetition &rep)
        : Placement(InitPlacementType()), trans_(trans), rep_(rep)
    {
    }
    ~RepPlacement() = default;
    RepPlacement(const RepPlacement &other) = default;
    RepPlacement(RepPlacement &&other) noexcept = default;
    RepPlacement &operator=(const RepPlacement &p) = default;
    RepPlacement &operator=(RepPlacement &&p) noexcept = default;

    size_t SizeImpl() const { return rep_.Size(); }

    BoxI BoundingBoxImpl(const BoxI &box_base) const
    {
        BoxI inst_box;
        if (box_base.Empty()) {
            return inst_box;
        }
        inst_box = box_base.Transformed(trans_);
        BoxI rep_box = rep_.BoundingBox();
        inst_box.Set(inst_box.BottomLeft() + rep_box.BottomLeft(), inst_box.TopRight() + rep_box.TopRight());
        return inst_box;
    }

    template <class OtherTransType>
    bool TransformImpl(const OtherTransType &trans)
    {
        static_assert(
            std::is_same_v<OtherTransType, SimpleTransformation> || std::is_same_v<OtherTransType, Transformation>,
            "Just support SimpleTransformation or Transformation");
        if constexpr (std::is_same_v<TransType, SimpleTransformation> &&
                      std::is_same_v<OtherTransType, Transformation>) {
            return false;
        } else {
            trans_ = Compose(trans, trans_);
            rep_.TransformWithoutTranslation(trans);
            return true;
        }
    }

    template <class OtherTransType>
    UniquePtrType TransformedImpl(const OtherTransType &trans) const;

    TransType TransImpl(size_t index) const
    {
        if (index >= Size()) {
            return TransType();
        }
        TransType ret = trans_;
        ret.Translation() += rep_.Offset(index);
        return ret;
    }

    const TransType &RawTransImpl() const { return trans_; }

    const Repetition &RawRepetitionImpl() const { return rep_; }

    std::string ToStringImpl() const
    {
        if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
            return "placement_type: Simple Repetition Ref\n" + trans_.ToString() + "\n" + rep_.ToString();
        } else {
            return "placement_type: Repetition Ref\n" + trans_.ToString() + "\n" + rep_.ToString();
        }
    }

    void SetTransImpl(const TransType &trans) { trans_ = trans; }

    void SetRepetitionImpl(const Repetition &rep) { rep_ = rep; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    /**
     * @brief 根据模板参数生成对应的 PlacementType
     */
    static constexpr PlacementType InitPlacementType()
    {
        if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
            return kSimpleRepRef;
        } else if constexpr (std::is_same_v<TransType, Transformation>) {
            return kRepRef;
        } else {
            return kInvalidPlacement;
        }
    }

private:
    TransType trans_;
    Repetition rep_;
};

using SimpleSingleRef = SinglePlacement<SimpleTransformation>;
using SingleRef = SinglePlacement<Transformation>;
using SimpleRepRef = RepPlacement<SimpleTransformation>;
using RepRef = RepPlacement<Transformation>;

// 为了使得以下这些函数能够正常 inline 以提高效率，这里不使用表驱动方法替换 switch case
inline Placement::UniquePtrType Placement::CopyPlacement(const Placement *placement)
{
    if (placement == nullptr) {
        return nullptr;
    }
    switch (placement->Type()) {
        case kSimpleSRef:
            return UniquePtrType(new SimpleSingleRef(*static_cast<const SimpleSingleRef *>(placement)));
        case kSRef:
            return UniquePtrType(new SingleRef(*static_cast<const SingleRef *>(placement)));
        case kSimpleRepRef:
            return UniquePtrType(new SimpleRepRef(*static_cast<const SimpleRepRef *>(placement)));
        case kRepRef:
            return UniquePtrType(new RepRef(*static_cast<const RepRef *>(placement)));
        default:
            return nullptr;
    }
}

inline void Placement::Deleter::operator()(Placement *placement)
{
    if (placement != nullptr) {
        switch (placement->Type()) {
            case kSimpleSRef:
                delete static_cast<SimpleSingleRef *>(placement);
                break;
            case kSRef:
                delete static_cast<SingleRef *>(placement);
                break;
            case kSimpleRepRef:
                delete static_cast<SimpleRepRef *>(placement);
                break;
            case kRepRef:
                delete static_cast<RepRef *>(placement);
                break;
            default:  // 应在编译期控制不生成 Invalid 类型的 placement
                break;
        }
    }
}

inline size_t Placement::Size() const
{
    switch (placement_type_) {
        case kSimpleSRef:
            return static_cast<const SimpleSingleRef *>(this)->SizeImpl();
        case kSRef:
            return static_cast<const SingleRef *>(this)->SizeImpl();
        case kSimpleRepRef:
            return static_cast<const SimpleRepRef *>(this)->SizeImpl();
        case kRepRef:
            return static_cast<const RepRef *>(this)->SizeImpl();
        default:
            return 0;
    }
}

inline BoxI Placement::BoundingBox(const BoxI &box_base) const
{
    switch (placement_type_) {
        case kSimpleSRef:
            return static_cast<const SimpleSingleRef *>(this)->BoundingBoxImpl(box_base);
        case kSRef:
            return static_cast<const SingleRef *>(this)->BoundingBoxImpl(box_base);
        case kSimpleRepRef:
            return static_cast<const SimpleRepRef *>(this)->BoundingBoxImpl(box_base);
        case kRepRef:
            return static_cast<const RepRef *>(this)->BoundingBoxImpl(box_base);
        default:
            return BoxI();
    }
}

template <class TransType>
inline bool Placement::Transform(const TransType &trans)
{
    switch (placement_type_) {
        case kSimpleSRef:
            return static_cast<SimpleSingleRef *>(this)->TransformImpl(trans);
        case kSRef:
            return static_cast<SingleRef *>(this)->TransformImpl(trans);
        case kSimpleRepRef:
            return static_cast<SimpleRepRef *>(this)->TransformImpl(trans);
        case kRepRef:
            return static_cast<RepRef *>(this)->TransformImpl(trans);
        default:
            return false;
    }
}

template <class TransType>
inline Placement::UniquePtrType Placement::Transformed(const TransType &trans) const
{
    switch (placement_type_) {
        case kSimpleSRef:
            return static_cast<const SimpleSingleRef *>(this)->TransformedImpl(trans);
        case kSRef:
            return static_cast<const SingleRef *>(this)->TransformedImpl(trans);
        case kSimpleRepRef:
            return static_cast<const SimpleRepRef *>(this)->TransformedImpl(trans);
        case kRepRef:
            return static_cast<const RepRef *>(this)->TransformedImpl(trans);
        default:
            return nullptr;
    }
}

inline TransformationVar Placement::Trans(size_t index) const
{
    switch (placement_type_) {
        case kSimpleSRef:
            return static_cast<const SimpleSingleRef *>(this)->TransImpl(index);
        case kSRef:
            return static_cast<const SingleRef *>(this)->TransImpl(index);
        case kSimpleRepRef:
            return static_cast<const SimpleRepRef *>(this)->TransImpl(index);
        case kRepRef:
            return static_cast<const RepRef *>(this)->TransImpl(index);
        default:
            return SimpleTransformation();
    }
}

inline TransformationVar Placement::RawTrans() const
{
    switch (placement_type_) {
        case kSimpleSRef:
            return static_cast<const SimpleSingleRef *>(this)->RawTransImpl();
        case kSRef:
            return static_cast<const SingleRef *>(this)->RawTransImpl();
        case kSimpleRepRef:
            return static_cast<const SimpleRepRef *>(this)->RawTransImpl();
        case kRepRef:
            return static_cast<const RepRef *>(this)->RawTransImpl();
        default:
            return SimpleTransformation();
    }
}

inline const Repetition *Placement::RawRepetition() const
{
    switch (placement_type_) {
        case kSimpleSRef:
        case kSRef:
            return nullptr;
        case kSimpleRepRef:
            return &static_cast<const SimpleRepRef *>(this)->RawRepetitionImpl();
        case kRepRef:
            return &static_cast<const RepRef *>(this)->RawRepetitionImpl();
        default:
            return nullptr;
    }
}

inline std::string Placement::ToString() const
{
    switch (placement_type_) {
        case kSimpleSRef:
            return static_cast<const SimpleSingleRef *>(this)->ToStringImpl();
        case kSRef:
            return static_cast<const SingleRef *>(this)->ToStringImpl();
        case kSimpleRepRef:
            return static_cast<const SimpleRepRef *>(this)->ToStringImpl();
        case kRepRef:
            return static_cast<const RepRef *>(this)->ToStringImpl();
        default:
            return "placement_type: Invalid Ref";
    }
}

class PlacementBuilder {
public:
    PlacementBuilder() = default;
    ~PlacementBuilder() = default;
    PlacementBuilder(const PlacementBuilder &other) = default;
    PlacementBuilder(PlacementBuilder &&other) noexcept = default;
    PlacementBuilder &operator=(const PlacementBuilder &p) = default;
    PlacementBuilder &operator=(PlacementBuilder &&p) noexcept = default;

    /**
     * @brief 通过 Repetition 类构建 Repetition，并更新 PlacementType
     */
    void BuildRepetition(const Repetition &rep)
    {
        rep_ = rep;
        UpdateRepetitionType();
    }

    /**
     * @brief 通过 ArrayInfo、OrdinaryVectorInfo、HorizontalVectorInfo 或 VerticalVectorInfo 构建 Repetition
     */
    template <class OffsetsType>
    void BuildRepetition(const OffsetsType &offsets)
    {
        BuildRepetition(Repetition(offsets));
    }

    /**
     * @brief 通过行、列信息构建 Repetition
     */
    void BuildRepetition(uint32_t rows, uint32_t cols, const VectorI &offset_row, const VectorI &offset_col)
    {
        BuildRepetition(Repetition(ArrayInfo(rows, cols, offset_row, offset_col)));
    }

    /**
     * @brief 通过 SimpleTransformation 类构建 SimpleTransformation
     */
    void BuildTransformation(const SimpleTransformation &simple_trans)
    {
        simple_trans_ = simple_trans;
        if (placement_type_ == kSRef) {
            placement_type_ = kSimpleSRef;
        } else if (placement_type_ == kRepRef) {
            placement_type_ = kSimpleRepRef;
        }
    }

    /**
     * @brief 通过 Transformation 类构建 Transformation
     */
    void BuildTransformation(const Transformation &trans)
    {
        trans_ = trans;
        if (placement_type_ == kSimpleSRef) {
            placement_type_ = kSRef;
        } else if (placement_type_ == kSimpleRepRef) {
            placement_type_ = kRepRef;
        }
    }

    /**
     * @brief 通过x轴和y轴的坐标值构建 SimpleTransformation
     */
    void BuildTransformation(int32_t x, int32_t y) { BuildTransformation(SimpleTransformation({x, y})); }

    /**
     * @brief 通过位移坐标值、旋转角度和放大倍数构建 Transformation
     */
    void BuildTransformation(double x, double y, RotationType rotation, double mag)
    {
        BuildTransformation(Transformation({x, y}, rotation, mag));
    }

    /**
     * @brief 通过内部的 PlacementType 变量构建 std::unique_ptr<Placement, Placement::DeleterType>
     *
     * @return std::unique_ptr<Placement, Placement::DeleterType> Placement 指针，可用于构建 Instance
     */
    Placement::UniquePtrType GetPlacement()
    {
        switch (placement_type_) {
            case kSimpleSRef:
                return Placement::UniquePtrType(new SimpleSingleRef(simple_trans_));
            case kSRef:
                return Placement::UniquePtrType(new SingleRef(trans_));
            case kSimpleRepRef:
                return Placement::UniquePtrType(new SimpleRepRef(simple_trans_, rep_));
            case kRepRef:
                return Placement::UniquePtrType(new RepRef(trans_, rep_));
            default:
                return nullptr;
        }
    }

    /**
     * @brief 重置 PlacementBuilder，只需要重置 simple_trans_ 和 placement_type_
     */
    void Reset()
    {
        simple_trans_.Set({0, 0});
        placement_type_ = kSimpleSRef;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    /**
     * @brief 更新 PlacementType 中的 Repetition 类型信息
     */
    void UpdateRepetitionType()
    {
        if (placement_type_ == kSimpleSRef) {
            placement_type_ = kSimpleRepRef;
        } else if (placement_type_ == kSRef) {
            placement_type_ = kRepRef;
        }
    }

private:
    SimpleTransformation simple_trans_;
    Transformation trans_;
    Repetition rep_;
    PlacementType placement_type_{kSimpleSRef};
};

template <class TransType>
template <class OtherTransType>
inline Placement::UniquePtrType SinglePlacement<TransType>::TransformedImpl(const OtherTransType &trans) const
{
    PlacementBuilder builder;
    builder.BuildTransformation(Compose(trans, trans_));
    return builder.GetPlacement();
}

template <class TransType>
template <class OtherTransType>
inline Placement::UniquePtrType RepPlacement<TransType>::TransformedImpl(const OtherTransType &trans) const
{
    PlacementBuilder builder;
    builder.BuildTransformation(Compose(trans, trans_));
    builder.BuildRepetition(rep_.TransformedWithoutTranslation(trans));
    return builder.GetPlacement();
}

}  // namespace medb2

#endif  // MEDB_PLACEMENT_H