/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_TRAITS_H
#define MEDB_TRAITS_H

#include "medb/box.h"
#include "medb/enums.h"
#include "medb/path.h"
#include "medb/polygon.h"
#include "medb/shape_repetition.h"
#include "medb/text.h"

namespace medb2 {

template <class ShapesType>
class ShapeTraits {};

template <>
class ShapeTraits<BoxI> {
public:
    using VectorType = MeVector<BoxI>;
    using ShapeType = BoxI;
    static constexpr ElementType enum_value_{ElementType::kBox};
};

template <>
class ShapeTraits<PolygonI> {
public:
    using VectorType = MeVector<PolygonI>;
    using ShapeType = PolygonI;
    static constexpr ElementType enum_value_{ElementType::kPolygon};
};

template <>
class ShapeTraits<Text> {
public:
    using VectorType = MeVector<Text>;
    using ShapeType = Text;
    static constexpr ElementType enum_value_{ElementType::kText};
};

template <>
class ShapeTraits<PathI> {
public:
    using VectorType = MeVector<PathI>;
    using ShapeType = PathI;
    static constexpr ElementType enum_value_{ElementType::kPath};
};

template <>
class ShapeTraits<ShapeRepetition<BoxI>> {
public:
    using VectorType = MeVector<ShapeRepetition<BoxI>>;
    using ShapeType = ShapeRepetition<BoxI>;
    static constexpr ElementType enum_value_{ElementType::kBoxRep};
};

template <>
class ShapeTraits<ShapeRepetition<PolygonI>> {
public:
    using VectorType = MeVector<ShapeRepetition<PolygonI>>;
    using ShapeType = ShapeRepetition<PolygonI>;
    static constexpr ElementType enum_value_{ElementType::kPolygonRep};
};

template <size_t N>
class ShapeIndexTraits {};

template <>
class ShapeIndexTraits<EToI(ElementType::kBox)> {
public:
    using Type = ShapeTraits<BoxI>;
};

template <>
class ShapeIndexTraits<EToI(ElementType::kPolygon)> {
public:
    using Type = ShapeTraits<PolygonI>;
};

template <>
class ShapeIndexTraits<EToI(ElementType::kText)> {
public:
    using Type = ShapeTraits<Text>;
};

template <>
class ShapeIndexTraits<EToI(ElementType::kPath)> {
public:
    using Type = ShapeTraits<PathI>;
};

template <>
class ShapeIndexTraits<EToI(ElementType::kBoxRep)> {
public:
    using Type = ShapeTraits<ShapeRepetition<BoxI>>;
};

template <>
class ShapeIndexTraits<EToI(ElementType::kPolygonRep)> {
public:
    using Type = ShapeTraits<ShapeRepetition<PolygonI>>;
};

}  // namespace medb2

#endif  // MEDB_TRAITS_H