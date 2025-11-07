/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_ENUMS_H
#define MEDB_ENUMS_H

#include <cstdint>
#include <type_traits>

namespace medb2 {
enum class ShapeManhattanType : uint8_t { kUnknown = 0, kManhattan = 1, kOctangular = 2, kAnyAngle = 3 };

enum class FontType : int8_t {
    kInvalidFont = -1,
    kFont0 = 0,
    kFont1,
    kFont2,
    kFont3,
};

enum class VerticalAlignType : int8_t {
    kInvalidVerticalAlign = -1,
    kVerticalAlignTop = 0,
    kVerticalAlignMiddle,
    kVerticalAlignBottom,
};

enum class HorizonAlignType : int8_t {
    kInvalidHorizonAlign = -1,
    kHorizonAlignLeft = 0,
    kHorizonAlignCenter,
    kHorizonAlignRight,
};

enum class ElementType : uint8_t {
    kBox = 0,
    kPolygon,
    kText,
    kPath,
    kBoxRep,
    kPolygonRep,
    kShapeNum,
    kInstance = kShapeNum,
    kElementNum
};

enum BooleanType : uint8_t { kOr = 0, kAnd, kSub, kXor };

enum class GeometryOperationType : uint8_t { kBoolean = 0, kCompare, kResize };

enum ManhattanCompressType : uint8_t {
    kNoCompress = 0,  // 不压缩，常规方式存储点集。矩形存四个点
    kCompressH,       // 压缩方式存储点集。间隔存储，第一个点与下一个省略点 y 坐标相同。
                      // 对应 OASIS 的 point lists 的 type 0 存储方式(horizontal-first)
    kCompressV,       // 压缩方式存储点集。间隔存储，第一个点与下一个省略点 x 坐标相同。
                      // 对应 OASIS 的 point lists 的 type 1 存储方式(vertical-first)
};

enum class DensityType : uint8_t {
    kDensityGrid = 0,
    kDensityRandom,
};

enum RotationType : uint8_t {
    kRotation0 = 0,  // 表示逆时针旋转 0 度
    kRotation90,     // 表示逆时针旋转 90 度
    kRotation180,    // 表示逆时针旋转 180 度
    kRotation270,    // 表示逆时针旋转 270 度
    kRotation360,    // 表示逆时针旋转 360 度
};

enum AngleType : uint8_t {
    kDegree0 = 0,  // 角度为 0 度
    kDegree90,     // 角度为 90 度
    kDegree180,    // 角度为 180 度
    kDegree270,    // 角度为 270 度
    kOtherAngle
};

enum PlacementType : uint8_t {
    kInvalidPlacement = 0,  // 表示非法的 Placement
    kSimpleSRef,            // 表示只支持平移的 SinglePlacement
    kSRef,                  // 表示支持平移、旋转、镜像、缩放的 SinglePlacement
    kSimpleRepRef,          // 表示只支持平移的 RepetitionPlacement
    kRepRef,                // 表示支持平移、旋转、镜像、缩放的 RepetitionPlacement
};

enum CompressAlgoType : uint8_t {
    kNoneCompressAlgo = 0,
    kVectorCompressAlgo,
    kArrayCompressAlgo,
};

enum BoundaryDirection : uint8_t {
    kTopBoundary = 0,
    kBottomBoundary,
    kLeftBoundary,
    kRightBoundary,
    kBoundaryDirectionCount
};

enum class SpatialQueryMode {
    kAccurate,  // 根据 region 和图形的 BoundingBox 做精确的相交判断，得到相应的图形
    kSimple,    // 根据 region 找到对应格子，返回格子内所有的图形
};

enum class ElementPropertyType : uint8_t {
    kArea = 0  // 面积
};

enum class QueryElementType { kOnlyShape, kOnlyInstance, kShapeAndInstance };

enum class OrientationType : uint8_t {
    kHorizontal = 0,
    kVertical,
};

enum class DeleteCellType : uint8_t {
    kShallow,  // 仅删除指定 Cell，不会删除后代 Cell
    kDeep      // 删除指定 Cell 以及不再被使用的子 Cell
};

enum class CornerType : uint8_t {
    kConvex = 0,  // 角度范围为（0°, 180°）
    kConcave,     // 角度范围为（180°, 360°）
    kStraight,    // 平角，角度为 0° 或 180°
};

template <class EnumType>
constexpr typename std::underlying_type<EnumType>::type EToI(EnumType e) noexcept
{
    return static_cast<typename std::underlying_type<EnumType>::type>(e);
}

}  // namespace medb2
#endif