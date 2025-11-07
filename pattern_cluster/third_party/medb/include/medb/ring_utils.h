/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_RING_UTILS_H
#define MEDB_RING_UTILS_H

#include <vector>

#include "medb/enums.h"
#include "medb/point_utils.h"
#include "medb/ring.h"

namespace medb2 {

template <class CoordType>
void MEDB_API SplitHoles(const std::vector<Point<CoordType>> &input, const ShapeManhattanType &manh_type,
                         MeVector<Ring<CoordType>> &output);

template <class CoordType>
void MEDB_API MergeHoles(const MeVector<Ring<CoordType>> &input, std::vector<Point<CoordType>> &output);

template <class CoordType>
ShapeManhattanType GetManhattanType(const Ring<CoordType> &ring)
{
    auto points = ring.Points();
    return GetPointsType(points.data(), static_cast<uint32_t>(points.size()));
}

template <class CoordType>
ShapeManhattanType GetManhattanType(const MeVector<Ring<CoordType>> &rings)
{
    ShapeManhattanType result_type = ShapeManhattanType::kManhattan;
    for (const auto &ring : rings) {
        ShapeManhattanType ring_manhattan_type = GetManhattanType(ring);
        if (ring_manhattan_type == ShapeManhattanType::kOctangular) {
            result_type = ShapeManhattanType::kOctangular;
        } else if (ring_manhattan_type == ShapeManhattanType::kAnyAngle) {
            result_type = ShapeManhattanType::kAnyAngle;
            break;
        }
    }
    return result_type;
}

// 对一个表示manhattan图形的常规ring，进行压缩，要求input一定是一个表示manhattan图形的常规ring
template <class CoordType>
void CompressManhattanRings(const MeVector<Ring<CoordType>> &ring_input, MeVector<Ring<CoordType>> &ring_output)
{
    std::vector<medb2::Point<CoordType>> output;
    ManhattanCompressType compress_type =
        ring_input[0].Raw()[0].Y() == ring_input[0].Raw()[1].Y() ? kCompressH : kCompressV;
    PointsFlag flag;
    flag.manhattan_type = ShapeManhattanType::kManhattan;
    flag.compress_type = compress_type;
    for (const auto &r_input : ring_input) {
        CompressManhattanPoints(r_input.Raw(), output);
        Ring<CoordType> ring(output, flag);
        ring_output.emplace_back(ring);
    }
}

}  // namespace medb2

#endif