/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_GEOMETRY_DATA_H
#define MEDB_GEOMETRY_DATA_H

#include "allocator.h"
#include "enums.h"

namespace medb2 {

class DirtyFlag {
public:
    constexpr DirtyFlag() : bounding_box_flag_(true), spatial_index_flag_(true), layer_flag_(true) {}
    constexpr DirtyFlag(bool bounding_box_flag, bool spatial_index_flag, bool layer_flag)
        : bounding_box_flag_(bounding_box_flag), spatial_index_flag_(spatial_index_flag), layer_flag_(layer_flag)
    {
    }

    bool BoundingBoxFlag() const { return bounding_box_flag_; }
    void SetBoundingBoxFlag(bool flag) { bounding_box_flag_ = flag; }

    bool SpatialIndexFlag() const { return spatial_index_flag_; }
    void SetSpatialIndexFlag(bool flag) { spatial_index_flag_ = flag; }

    bool LayerFlag() const { return layer_flag_; }
    void SetLayerFlag(bool flag) { layer_flag_ = flag; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    bool bounding_box_flag_ : 1;
    bool spatial_index_flag_ : 1;
    bool layer_flag_ : 1;
};
inline constexpr DirtyFlag kAllDirty;
inline constexpr DirtyFlag kBoundingBoxDirty(true, false, false);
inline constexpr DirtyFlag kSpatialIndexDirty(false, true, false);
inline constexpr DirtyFlag kLayerDirty(false, false, true);
inline constexpr DirtyFlag kBoundingBoxSpatialIndexDirty(true, true, false);

struct PointsFlag {
    ManhattanCompressType compress_type : 2;
    ShapeManhattanType manhattan_type : 2;
    // 表示点集中是否含有重复点或共线点，仅用于传参，Polygon 中的 points_flag 的这个变量始终为 false
    bool has_duplicate_or_collinear : 1;
    bool operator==(const PointsFlag &other) const
    {
        return compress_type == other.compress_type && manhattan_type == other.manhattan_type &&
               has_duplicate_or_collinear == other.has_duplicate_or_collinear;
    }
    bool operator!=(const PointsFlag &other) const { return !(*this == other); }
    PointsFlag()
        : compress_type(kNoCompress), manhattan_type(ShapeManhattanType::kUnknown), has_duplicate_or_collinear(true)
    {
    }
};

}  // namespace medb2
#endif  // MEDB_GEOMETRY_DATA_H
