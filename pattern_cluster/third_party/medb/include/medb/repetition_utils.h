/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_REPETITION_UTILS_H
#define MEDB_REPETITION_UTILS_H

#include <array>
#include <unordered_set>
#include "medb/box.h"
#include "medb/point.h"
#include "medb/polygon.h"
#include "medb/repetition.h"
#include "medb/vector_utils.h"

namespace std {
template <>
class hash<std::pair<uint32_t, medb2::VectorI>> {
public:
    size_t operator()(const std::pair<uint32_t, medb2::VectorI> &line_rep) const
    {
        size_t seed = 0;
        medb2::HashCombine(seed, line_rep.first);
        medb2::HashCombine(seed, line_rep.second);
        return seed;
    }
};
}  // namespace std
namespace medb2 {

inline constexpr std::array<std::pair<bool (*)(uint32_t), CompressAlgoType>, 3> kBoxAlgos = {
    std::make_pair([](uint32_t compress_level) { return compress_level == 0; }, kNoneCompressAlgo),
    std::make_pair([](uint32_t compress_level) { return compress_level == 1; }, kVectorCompressAlgo),
    std::make_pair([](uint32_t compress_level) { return compress_level >= 2; }, kArrayCompressAlgo)};

inline constexpr std::array<std::pair<bool (*)(uint32_t), CompressAlgoType>, 3> kPolygonAlgos = {
    std::make_pair([](uint32_t compress_level) { return compress_level <= 2; }, kNoneCompressAlgo),
    std::make_pair([](uint32_t compress_level) { return compress_level == 3; }, kVectorCompressAlgo),
    std::make_pair([](uint32_t compress_level) { return compress_level >= 4; }, kArrayCompressAlgo)};

static inline CompressAlgoType BoxCompressAlgoType(uint32_t compress_level)
{
    for (auto &box_algo : kBoxAlgos) {
        if (box_algo.first(compress_level)) {
            return box_algo.second;
        }
    }
    return kNoneCompressAlgo;
}

static inline CompressAlgoType PolygonCompressAlgoType(uint32_t compress_level)
{
    for (auto &poly_algo : kPolygonAlgos) {
        if (poly_algo.first(compress_level)) {
            return poly_algo.second;
        }
    }
    return kNoneCompressAlgo;
}

struct ClusterX {
    bool operator()(const VectorI &a, const VectorI &b) const
    {
        return (a.Y() != b.Y()) ? (a.Y() < b.Y()) : (a.X() < b.X());
    }
};

struct ClusterY {
    bool operator()(const VectorI &a, const VectorI &b) const
    {
        return (a.X() != b.X()) ? (a.X() < b.X()) : (a.Y() < b.Y());
    }
};

class Compressor {
public:
    using VectorIter = MeVector<VectorI>::iterator;
    using LineRepType = std::pair<uint32_t, VectorI>;

public:
    void AddVector(const VectorI &vector) { vectors_.push_back(vector); }

    void AddVector(VectorI &&vector) { vectors_.emplace_back(vector); }

    const MeVector<VectorI> &Vectors() const { return vectors_; }

    void SetVectors(const MeVector<VectorI> &vectors) { vectors_ = vectors; }

    MeVector<std::pair<VectorI, Repetition>> Compress(CompressAlgoType algo_type)
    {
        MeVector<std::pair<VectorI, Repetition>> result;
        if (algo_type == kNoneCompressAlgo) {
            return result;
        }
        if (algo_type == kArrayCompressAlgo) {
            ArrayCompressAlgo(result);
        }
        VectorCompressAlgo(result);
        return result;
    }

private:
    void SortVectors()
    {
        if (sorted_) {
            return;
        }
        MeUnorderedSet<int32_t> count_x;
        MeUnorderedSet<int32_t> count_y;
        for (const auto &vector : vectors_) {
            count_x.insert(vector.X());
            count_y.insert(vector.Y());
        }
        x_first_ = count_x.size() > count_y.size();

        if (x_first_) {
            std::sort(vectors_.begin(), vectors_.end(), ClusterX());
        } else {
            std::sort(vectors_.begin(), vectors_.end(), ClusterY());
        }
        sorted_ = true;
    }

    void VectorCompressAlgo(MeVector<std::pair<VectorI, Repetition>> &result)
    {
        // vector_ 中剩余的偏移向量只有一个时，不构造 Repetition，需要外部对其进行处理
        if (vectors_.size() <= 1) {
            return;
        }
        SortVectors();
        VectorIter remaining_end = vectors_.begin();
        VectorIter beg = vectors_.begin();
        VectorIter line_end = vectors_.begin();
        while (beg < vectors_.end()) {
            line_end = FindLineEnd(beg, vectors_, x_first_);
            // 一行或一列的偏移向量数量达到一定值之后，优先这一行或一列构造成一个 VectorRepetition
            if (beg + kMinLineRepetitionSize <= line_end) {
                MakeLineVectorRep(beg, line_end, result);
                beg = line_end;
            } else {
                // 将剩余的部分放到数组的前端
                while (beg < line_end) {
                    *remaining_end = *beg;
                    ++remaining_end;
                    ++beg;
                }
            }
        }
        vectors_.erase(remaining_end, vectors_.end());

        if (vectors_.size() <= 1) {
            return;
        }
        MakeVectorRep(vectors_.begin(), vectors_.end(), result);
        vectors_.clear();
    }

    void MakeLineVectorRep(VectorIter beg, VectorIter end, MeVector<std::pair<VectorI, Repetition>> &result)
    {
        // beg 到 end 中的所有元素至少在一个轴向上是相等的，因此都可以使用 ClusterX 进行排序
        std::sort(beg, end, ClusterX());
        MeVector<int32_t> coords;
        coords.reserve(end - beg);
        for (VectorIter iter = beg; iter < end; ++iter) {
            coords.emplace_back(x_first_ ? iter->X() - beg->X() : iter->Y() - beg->Y());
        }
        if (x_first_) {
            result.emplace_back(std::make_pair(*beg, Repetition(HorizontalVectorInfo(coords))));
        } else {
            result.emplace_back(std::make_pair(*beg, Repetition(VerticalVectorInfo(coords))));
        }
    }

    void MakeVectorRep(VectorIter beg, VectorIter end, MeVector<std::pair<VectorI, Repetition>> &result)
    {
        VectorI base_point = *beg;
        for (VectorIter iter = beg; iter < end; ++iter) {
            *iter -= base_point;
        }
        result.emplace_back(std::make_pair(base_point, Repetition(OrdinaryVectorInfo({beg, end}))));
    }

    void ArrayCompressAlgo(MeVector<std::pair<VectorI, Repetition>> &result)
    {
        SortVectors();
        // unordered_map<<Repetition 中的图形个数, Repetition 中的偏移向量>, Repetition 中初始图形相对于原点的偏移向量>
        MeUnorderedMap<LineRepType, MeVector<VectorI>> line_rep_map;
        VectorIter vector_head = vectors_.begin();
        VectorIter vector_end = vectors_.end();
        VectorIter remaining_vector_end = vectors_.begin();
        while (vector_head != vector_end) {
            VectorIter vector_line_end = FindLineEnd(vector_head, vectors_, x_first_);
            MakeLineRep(vector_head, vector_line_end, line_rep_map, remaining_vector_end);
            vector_head = vector_line_end;
        }
        vectors_.erase(remaining_vector_end, vectors_.end());

        for (int xy_loop2 = 1; xy_loop2 >= 0; --xy_loop2) {
            bool x_array_rep = x_first_ == (xy_loop2 == 0);
            for (auto &line_rep : line_rep_map) {
                auto &base_vector = line_rep.second;
                if (x_array_rep) {
                    std::sort(base_vector.begin(), base_vector.end(), ClusterX());
                } else {
                    std::sort(base_vector.begin(), base_vector.end(), ClusterY());
                }
                VectorIter rep_head = base_vector.begin();
                VectorIter rep_end = base_vector.end();
                VectorIter remaining_rep_end = base_vector.begin();
                while (rep_head != rep_end) {
                    VectorIter rep_line_end = FindLineEnd(rep_head, base_vector, x_array_rep);
                    MakeAndInsertArrayRepetition(line_rep.first, rep_head, rep_line_end, result, remaining_rep_end);
                    rep_head = rep_line_end;
                }
                base_vector.erase(remaining_rep_end, base_vector.end());
            }
        }
        InsertLineRep(line_rep_map, result, vectors_);
    }

    /*
     * @brief 从 beg 开始，在 vectors 中找到一行或一列的结尾的后一个迭代器。vectors 需先按 x_first 进行排序
     */
    VectorIter FindLineEnd(VectorIter beg, const MeVector<VectorI> &vectors, bool x_first)
    {
        int32_t base = x_first ? beg->Y() : beg->X();
        ++beg;
        while (beg != vectors.cend()) {
            if (base != (x_first ? beg->Y() : beg->X())) {
                break;
            }
            ++beg;
        }
        return beg;
    }

    /*
     * @brief 将一行或一列中能构成 LineRepetition 的提取到 line_reps 中，将剩余的一个个塞到 remaining_vector_end 中
     *
     * @param beg 一行或一列的开头
     * @param end 一行或一列的结尾的后一个迭代器
     * @param line_reps 压缩一行或一列 VectorI 的结果
     * @param remaining_vector_end 需要保留的偏移向量的末尾
     */
    void MakeLineRep(VectorIter beg, VectorIter end, MeUnorderedMap<LineRepType, MeVector<VectorI>> &line_rep_map,
                     VectorIter &remaining_vector_end)
    {
        while (beg < end) {
            auto cur = beg + 1;  // 前两个图形构成一个 Repetition 的起始图形
            if (cur == end) {
                // 此时 beg 为一行或一列中最后一个元素的迭代器，
                // 无法构成 Repetition，将其塞到数组的前端保留下来，之后再进行处理
                *remaining_vector_end = *beg;
                ++remaining_vector_end;
                return;
            }
            auto diff = SafeSub(*cur, *beg);  // 表示一个 Repetition 中的间隔向量
            int n = 2;                        // 表示一个 Repetition 中图形的个数
            ++cur;
            while (cur < end && SafeSub(*cur, *(cur - 1)) == diff) {
                ++cur;
                ++n;
            }
            // 如果一个 Repetition 中图形的个数为 2，则不将它们合成 Repetition，
            // 将当前 beg 指向的元素保留，并从下一个元素开始继续循环
            if (n == 2) {
                *remaining_vector_end = *beg;
                ++remaining_vector_end;
                ++beg;
            } else {
                line_rep_map[std::make_pair(n, VectorI(diff))].emplace_back(*beg);
                beg = beg + n;
            }
        }
    }

    /*
     * @brief 将 LineRep 构造成 ArrayRepetition 并插入到 result 中
     *
     * @param line_reps 待插入到结果集中的 line_rep 数组
     * @param result 整个压缩算法的返回结果
     * @param vectors 待处理的偏移向量的集合
     */
    void InsertLineRep(const MeUnorderedMap<LineRepType, MeVector<VectorI>> &line_rep_map,
                       MeVector<std::pair<VectorI, Repetition>> &result, MeVector<VectorI> &vectors)
    {
        for (const auto &line_rep : line_rep_map) {
            for (const auto &shape_offset : line_rep.second) {
                const auto &rep = line_rep.first;
                // line_rep 中至少含有 3 个偏移向量，array_rep 中至少含有 9 个偏移向量，因此只需在这里进行判断
                if (rep.first < kMinArrayRepetitionSize) {
                    for (uint32_t i = 0; i < rep.first; ++i) {
                        // 塞回 vectors_，会破坏顺序，可能会影响导出文件的大小，但影响很小，没必要重新排序
                        vectors.emplace_back(shape_offset + rep.second * i);
                    }
                } else {
                    result.emplace_back(
                        std::make_pair(shape_offset, Repetition(ArrayInfo(1, rep.first, {0, 0}, rep.second))));
                }
            }
        }
    }

    /*
     * @brief 将能结合成多行多列的 ArrayRepetition 提取到 result 中，将剩余的放回
     *
     * @param line_rep 一行或一列的 Repetition
     * @param beg 一行或一列的开头
     * @param end 一行或一列的结尾的后一个迭代器
     * @param result 整个压缩算法的返回结果
     * @param remaining_rep_end 需要保留的 line_rep 的末尾
     */
    void MakeAndInsertArrayRepetition(const LineRepType &line_rep, VectorIter beg, VectorIter end,
                                      MeVector<std::pair<VectorI, Repetition>> &result, VectorIter &remaining_rep_end)
    {
        while (beg < end) {
            auto cur = beg + 1;
            if (cur == end) {
                *remaining_rep_end = *beg;
                ++remaining_rep_end;
                return;
            }
            auto diff = SafeSub(*cur, *beg);
            int n = 2;  // 两个图形组合成一个 ArrayRepetition 的起始图形
            ++cur;
            while (cur < end && SafeSub(*cur, *(cur - 1)) == diff) {
                ++cur;
                ++n;
            }
            if (n == 2) {
                *remaining_rep_end = *beg;
                ++remaining_rep_end;
                ++beg;
            } else {
                result.emplace_back(
                    std::make_pair(*beg, Repetition(ArrayInfo(n, line_rep.first, VectorI(diff), line_rep.second))));
                beg = beg + n;
            }
        }
    }

private:
    MeVector<VectorI> vectors_;  // 存储所有图形相对于原点的偏移向量的容器
    bool x_first_ = false;       // 排序后才有效，表示排序的轴向，x_first_ 为 true 表示从小到大沿着 x 轴进行排序
    bool sorted_ = false;        // 表示是否调用过排序算法
};

}  // namespace medb2

#endif  // MEDB_REPETITION_UTILS_H