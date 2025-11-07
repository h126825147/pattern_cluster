/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_ELEMENT_H
#define MEDB_ELEMENT_H

#include <bitset>
#include <cmath>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "medb/box.h"
#include "medb/enums.h"
#include "medb/geometry_utils.h"
#include "medb/instance.h"
#include "medb/layer.h"
#include "medb/text.h"
#include "medb/traits.h"

namespace medb2 {
/*
 *Element
 *代表图形抽象类，内部私有void*成员变量为所指向具体图形数据的指针，并采用指针高位复用的方式，实现具体数据类型的表示。
 */
class Element {
    static constexpr uint8_t kWidth = 3;                   // Element 类型占用3个bit位，表示最多7种图形类型
    static constexpr uintptr_t kMask = (1 << kWidth) - 1;  // Element 类型bit位掩码：0B111
    // kBitsStep 表示类型掩码左移步数为61位。对象指针的高三位用作表示具体类型，64位系统，sizeof(uintptr_t *) * 8 -
    // kElementTypeBitsWidth = 61，代表类型占据63~61的高3位 :
    static constexpr uintptr_t kBitsStep = sizeof(uintptr_t *) * 8 - kWidth;
    static constexpr uintptr_t kBitsMask = (kMask << kBitsStep);  // Element图形类型掩码
    static constexpr uintptr_t kBoxBits = (static_cast<uintptr_t>(ElementType::kBox) << kBitsStep);  // Box类型bit位表示
    static constexpr uintptr_t kPolygonBits =
        (static_cast<uintptr_t>(ElementType::kPolygon) << kBitsStep);  // Polygon类型bit位表示
    static constexpr uintptr_t kTextBits =
        (static_cast<uintptr_t>(ElementType::kText) << kBitsStep);  // Text类型bit位表示
    static constexpr uintptr_t kPathBits =
        (static_cast<uintptr_t>(ElementType::kPath) << kBitsStep);  // Path类型bit位表示
    static constexpr uintptr_t kBoxRepBits =
        (static_cast<uintptr_t>(ElementType::kBoxRep) << kBitsStep);  // BoxRep类型bit位表示
    static constexpr uintptr_t kPolygonRepBits =
        (static_cast<uintptr_t>(ElementType::kPolygonRep) << kBitsStep);  // PolygonRep类型bit位表示
    static constexpr uintptr_t kInstanceBits =
        (static_cast<uintptr_t>(ElementType::kInstance) << kBitsStep);  // Instance类型bit位表示
    static constexpr uintptr_t kElementNumBits =
        (static_cast<uintptr_t>(ElementType::kElementNum)
         << kBitsStep);  // 除7种图形以外的其它类型bit位表示，表示 data 是个无效的数据

public:
    /**
     * @brief Element构造函数
     *
     * @param data    Element的任意类型图形数据指针
     * @param type    Element数据类型
     */
    Element(const void *data, ElementType type)
    {
        data_ = reinterpret_cast<uintptr_t>(data);
        switch (type) {
            case ElementType::kBox:
                data_ = data_ | kBoxBits;
                break;
            case ElementType::kPolygon:
                data_ = data_ | kPolygonBits;
                break;
            case ElementType::kText:
                data_ = data_ | kTextBits;
                break;
            case ElementType::kPath:
                data_ = data_ | kPathBits;
                break;
            case ElementType::kBoxRep:
                data_ = data_ | kBoxRepBits;
                break;
            case ElementType::kPolygonRep:
                data_ = data_ | kPolygonRepBits;
                break;
            case ElementType::kInstance:
                data_ = data_ | kInstanceBits;
                break;
            default:
                data_ = kElementNumBits;
                break;
        }
    }
    Element() = default;
    ~Element() = default;

    bool operator==(const Element &ref) const { return data_ == ref.data_; }
    bool operator<(const Element &ref) const { return data_ < ref.data_; }

    /**
     * @brief 将Element数据转换为期望的具体图形类型
     *
     * @return ShapeType* 转换后的图形数据指针对象
     */
    template <class ShapeType>
    const ShapeType *Cast() const
    {
        auto base_type = data_ & kBitsMask;
        if constexpr (std::is_same<ShapeType, Instance>::value) {
            if (base_type != kInstanceBits) {
                return nullptr;
            }
        } else {
            auto type_num = ShapeTraits<ShapeType>::enum_value_;
            if (base_type != static_cast<uintptr_t>(type_num) << kBitsStep) {
                return nullptr;
            }
        }
        void *base = reinterpret_cast<void *>(data_ & ~kBitsMask);
        return static_cast<ShapeType *>(base);
    }

    /**
     * @brief 将此对象重置为空
     */
    void Reset() { data_ = kElementNumBits; }

    /**
     * @brief 判断是否 type 类型的对象
     *
     * @param type 期望的类型
     * @return 如果是 type 类型则返回 true，否则返回 false
     */
    bool IsType(ElementType type) const { return (static_cast<uintptr_t>(type) << kBitsStep) == (data_ & kBitsMask); }

    /**
     * @brief 判断Element类型是否与期望的图形数据类型一致
     *
     * @param types 类型比较位图
     * @return bool 当前Element类型是否匹配
     */
    bool IsTypesMatch(const std::bitset<EToI(ElementType::kElementNum)> &types) const
    {
        auto base_type = (data_ & kBitsMask) >> kBitsStep;
        std::bitset<EToI(ElementType::kElementNum)> element_bits;
        element_bits.set(base_type);
        if ((types & element_bits) != 0) {
            return true;
        }
        return false;
    }

    /**
     * @brief 获取当前Element数据的BoundingBox
     *
     * @param layer
     * 默认Layer参数为空，当传入Layer参数时，表示当前类型是Instance，需要获取指定layer下的instance的BoundingBox
     * @return BoxI 当前Element的BoundingBox
     */
    BoxI BoundingBox(const std::optional<Layer> &layer = std::nullopt) const
    {
        BoxI element_box;
        auto bits_mask = data_ & kBitsMask;
        auto *base = reinterpret_cast<void *>(data_ & ~kBitsMask);
        switch (bits_mask) {
            case kBoxBits:
                element_box = GetBoundingBox(*(reinterpret_cast<BoxI *>(base)));
                break;
            case kPolygonBits:
                element_box = GetBoundingBox(*(reinterpret_cast<PolygonI *>(base)));
                break;
            case kPathBits:
                element_box = GetBoundingBox(*(reinterpret_cast<PathI *>(base)));
                break;
            case kBoxRepBits:
                element_box = GetBoundingBox(*(reinterpret_cast<ShapeRepetition<BoxI> *>(base)));
                break;
            case kPolygonRepBits:
                element_box = GetBoundingBox(*(reinterpret_cast<ShapeRepetition<PolygonI> *>(base)));
                break;
            case kInstanceBits:
                if (layer != std::nullopt) {
                    element_box = (reinterpret_cast<Instance *>(base))->GetBoundingBox(*layer);
                }
                break;
            default:
                break;
        }
        return element_box;
    }

    double Area() const
    {
        double area = 0.0;
        auto bits_mask = data_ & kBitsMask;
        auto *base = reinterpret_cast<void *>(data_ & ~kBitsMask);
        switch (bits_mask) {
            case kBoxBits:
                area = reinterpret_cast<BoxI *>(base)->Area();
                break;
            case kPolygonBits:
                area = reinterpret_cast<PolygonI *>(base)->Area();
                break;
            case kPathBits:
                area = reinterpret_cast<PathI *>(base)->ToPolygon().Area();
                break;
            case kBoxRepBits:
            case kPolygonRepBits:
            case kInstanceBits:
            default:
                break;
        }
        return area;
    }

    /**
     * @brief 判断当前对象是否空
     */
    bool Empty() const { return data_ == kElementNumBits; }

public:
    inline static const MeUnorderedSet<ElementType> kAllTypes = {
        ElementType::kBox,    ElementType::kPolygon,    ElementType::kText,    ElementType::kPath,
        ElementType::kBoxRep, ElementType::kPolygonRep, ElementType::kInstance};
    inline static const MeUnorderedSet<ElementType> kElementWithAreaTypes = {
        ElementType::kBox, ElementType::kPolygon, ElementType::kPath, ElementType::kBoxRep, ElementType::kPolygonRep};

private:
    uintptr_t data_{kElementNumBits};

    friend struct std::hash<Element>;
};
}  // namespace medb2
#endif  // MEDB_ELEMENT_H
