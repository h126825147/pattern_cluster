/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_CONST_H
#define MEDB_CONST_H

#include <cstdint>

namespace medb2 {

inline constexpr double kDegrees90 = 90.0;
inline constexpr double kDegrees180 = 180.0;
inline constexpr double kDegrees270 = 270.0;
inline constexpr double kDegrees360 = 360.0;

inline constexpr double kDoubleEPS = 1e-9;
inline constexpr float kFloatEPS = 1e-5f;

inline constexpr double kPI = 3.14159265358979323846;
inline constexpr double kSqrt2 = 1.41421356237309504880;

inline constexpr size_t kPolygonWithHoleMinPointCount = 7;
inline constexpr size_t kCompressPolygonWithHoleMinPointCount = 5;
inline constexpr size_t kRingMinPointCount = 3;
inline constexpr size_t kCompressRingMinPointCount = 2;
inline constexpr size_t kNeedFilterMinPointCount = 3;
inline constexpr size_t kRectanglePointSize = 4;
inline constexpr size_t kMinArrayRepetitionSize = 5;
inline constexpr size_t kMinLineRepetitionSize = 16;

inline constexpr size_t kDefaultThreadNum = 16;
inline constexpr size_t kDefaultWindowStep = 500000;

// 以下 kUpdateXXX 变量是用于 Update 函数的各 flag
inline constexpr uint16_t kUpdateBoundingBox = 1 << 0;   // 更新 Shapes 的 bounding box
inline constexpr uint16_t kUpdateSpatialIndex = 1 << 1;  // 更新 Shapes 的空间索引
inline constexpr uint16_t kUpdateLayer = 1 << 2;         // 从 Layout 对象更新 Layer 信息
inline constexpr uint16_t kUpdateTopCell = 1 << 3;       // 从 Layout 对象更新 Cell 信息

}  // namespace medb2
#endif  // MEDB_CONST_H
