
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_ARRAY_INFO_H
#define MEDB_ARRAY_INFO_H

#include "medb/box_utils.h"
#include "medb/point.h"
#include "medb/point_utils.h"
#include "medb/vector_utils.h"

namespace medb2 {

class ArrayInfo {
public:
    ArrayInfo() = default;
    ~ArrayInfo() = default;
    ArrayInfo(uint32_t rows, uint32_t cols, const Vector<int32_t> &offset_row, const Vector<int32_t> &offset_col)
        : rows_(rows), cols_(cols), offset_row_(offset_row), offset_col_(offset_col)
    {
    }

    /**
     * @brief 获取 ArrayInfo 中所有偏移向量指向的点的包围盒
     */
    BoxI BoundingBox() const
    {
        MeVector<PointI> points = {{0, 0},
                                   offset_row_ * (rows_ - 1),
                                   offset_col_ * (cols_ - 1),
                                   offset_row_ * (rows_ - 1) + offset_col_ * (cols_ - 1)};
        return GetBoundingBox(points);
    }

    /**
     * @brief 获取 ArrayInfo 中第 i 行，第 j 列的偏移向量
     */
    Vector<int32_t> Offset(uint32_t i, uint32_t j) const
    {
        if (i >= rows_ || j >= cols_) {
            return {0, 0};
        }
        return offset_row_ * i + offset_col_ * j;
    }

    /**
     * @brief 获取 ArrayInfo 中第 index 个偏移向量（行优先）
     */
    Vector<int32_t> Offset(size_t index) const
    {
        if (index >= rows_ * cols_) {
            return {0, 0};
        }
        return offset_row_ * (index / cols_) + offset_col_ * (index % cols_);
    }

    /**
     * @brief 对 ArrayInfo 进行原地 Transform，包含旋转、镜像、缩放，不包含位移
     *
     * @return const ArrayInfo& 返回 ArrayInfo 自身对象的引用
     */
    template <class TransType>
    const ArrayInfo &TransformWithoutTranslation(const TransType &trans)
    {
        if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
            return *this;
        }
        auto transform_offset_func = [&trans](VectorI &offset) {
            if (DoubleLess(trans.Magnification(), 0.0)) {
                offset.SetY(-offset.Y());
            }
            offset *= fabs(trans.Magnification());
            RotatePoint(offset, trans.Rotation());
        };
        transform_offset_func(offset_row_);
        transform_offset_func(offset_col_);
        return *this;
    }

    /**
     * @brief 对 ArrayInfo 的拷贝进行 Transform，包含旋转、镜像、缩放，不包含位移
     *
     * @return ArrayInfo 返回拷贝后并进行了 Transform 操作的 ArrayInfo
     */
    template <class TransType>
    ArrayInfo TransformedWithoutTranslation(const TransType &trans) const
    {
        ArrayInfo result = *this;
        result.TransformWithoutTranslation(trans);
        return result;
    }

    /**
     * @brief 获取所有在 region 中的偏移向量
     *  若第 i 行，第 j 列的偏移向量满足以下条件，则将其输出
     *  region_min_x <= offset_row_x * i + offset_col_x  * j <= region_max_x
     *  region_min_y <= offset_row_y * i + offset_col_y  * j <= region_max_y
     *
     * @return MeVector<VectorI> 返回所有在 region 中的偏移向量
     */
    MeVector<VectorI> RegionQuery(const BoxI &region) const
    {
        MeVector<VectorI> res;
        BoxI bbox = BoundingBox();
        if (IsContain(region, bbox)) {
            return GetAllOffset();
        }
        if (!IsIntersect(region, bbox)) {
            return MeVector<VectorI>();
        }

        Interval row_interval(0, rows_);
        Interval col_interval(0, cols_);

        // 求解 row_interval
        if (offset_col_.X() == 0) {
            row_interval.Update(region.Left(), offset_row_.X(), region.Right());
        } else if (offset_col_.Y() == 0) {
            row_interval.Update(region.Bottom(), offset_row_.Y(), region.Top());
        } else {
            // 消除 j，消元法求解 i 的区间
            int64_t a = static_cast<int64_t>(offset_col_.Y() > 0 ? region.Left() : region.Right()) * offset_col_.Y();
            int64_t c = static_cast<int64_t>(offset_col_.Y() > 0 ? region.Right() : region.Left()) * offset_col_.Y();
            a -= static_cast<int64_t>(offset_col_.X() > 0 ? region.Top() : region.Bottom()) * offset_col_.X();
            c -= static_cast<int64_t>(offset_col_.X() > 0 ? region.Bottom() : region.Top()) * offset_col_.X();
            row_interval.Update(a, CrossProduct(offset_row_, offset_col_), c);
        }

        VectorI vec_i = offset_row_ * row_interval.min_;
        for (int i = row_interval.min_; i < row_interval.max_; ++i) {
            // 求解 col_interval
            Interval temp = col_interval;
            temp.Update(region.Left() - vec_i.X(), offset_col_.X(), region.Right() - vec_i.X());
            temp.Update(region.Bottom() - vec_i.Y(), offset_col_.Y(), region.Top() - vec_i.Y());
            for (int j = temp.min_; j < temp.max_; ++j) {
                res.emplace_back(vec_i + offset_col_ * j);
            }
            vec_i += offset_row_;
        }
        return res;
    }

    /**
     * @brief 检查当前对象中的偏移向量是否有落在 region 中的
     *
     * @return bool true 表示有偏移向量落在 region 中
     */
    bool HasOffsetIn(const BoxI &region) const
    {
        BoxI bbox = BoundingBox();
        if (!IsIntersect(region, bbox)) {
            return false;
        }

        Interval row_interval(0, rows_);
        Interval col_interval(0, cols_);

        // 求解 row_interval
        if (offset_col_.X() == 0) {
            row_interval.Update(region.Left(), offset_row_.X(), region.Right());
        } else if (offset_col_.Y() == 0) {
            row_interval.Update(region.Bottom(), offset_row_.Y(), region.Top());
        } else {
            // 消除 j，消元法求解 i 的区间
            int64_t a = static_cast<int64_t>(offset_col_.Y() > 0 ? region.Left() : region.Right()) * offset_col_.Y();
            int64_t c = static_cast<int64_t>(offset_col_.Y() > 0 ? region.Right() : region.Left()) * offset_col_.Y();
            a -= static_cast<int64_t>(offset_col_.X() > 0 ? region.Top() : region.Bottom()) * offset_col_.X();
            c -= static_cast<int64_t>(offset_col_.X() > 0 ? region.Bottom() : region.Top()) * offset_col_.X();
            row_interval.Update(a, CrossProduct(offset_row_, offset_col_), c);
        }

        VectorI vec_i = offset_row_ * row_interval.min_;
        for (int i = row_interval.min_; i < row_interval.max_; ++i) {
            // 求解 col_interval
            Interval temp = col_interval;
            temp.Update(region.Left() - vec_i.X(), offset_col_.X(), region.Right() - vec_i.X());
            temp.Update(region.Bottom() - vec_i.Y(), offset_col_.Y(), region.Top() - vec_i.Y());
            if (temp.min_ < temp.max_) {
                return true;
            }
            vec_i += offset_row_;
        }
        return false;
    }

    /**
     * @brief 获取 ArrayInfo 中行偏移向量
     */
    const Vector<int32_t> &OffsetRow() const { return offset_row_; }

    /**
     * @brief 获取 ArrayInfo 中列偏移向量
     */
    const Vector<int32_t> &OffsetCol() const { return offset_col_; }

    /**
     * @brief 获取 ArrayInfo 中图形的行数
     */
    uint32_t Rows() const { return rows_; }

    /**
     * @brief 获取 ArrayInfo 中图形的列数
     */
    uint32_t Cols() const { return cols_; }

    /**
     * @brief 获取 ArrayInfo 中图形的总数
     */
    size_t Size() const { return static_cast<size_t>(rows_) * cols_; }

    /**
     * @brief 设置 ArrayInfo 中行偏移向量
     */
    void SetOffsetRow(const Vector<int32_t> &offset_row) { offset_row_ = offset_row; }

    /**
     * @brief 设置 ArrayInfo 中列偏移向量
     */
    void SetOffsetCol(const Vector<int32_t> &offset_col) { offset_col_ = offset_col; }

    /**
     * @brief 设置 ArrayInfo 中图形的行数
     */
    void SetRows(uint32_t rows) { rows_ = rows; }

    /**
     * @brief 设置 ArrayInfo 中图形的列数
     */
    void SetCols(uint32_t cols) { cols_ = cols; }

    std::string ToString() const
    {
        std::string str;
        str.append("rows: ")
            .append(std::to_string(rows_))
            .append("\n")
            .append("cols: ")
            .append(std::to_string(cols_))
            .append("\n")
            .append("offset_row: ")
            .append(offset_row_.ToString())
            .append("\n")
            .append("offset_col: ")
            .append(offset_col_.ToString())
            .append("\n");
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    // 表示一个区间内的整数，min_ <= x < max_
    struct Interval {
        Interval(int32_t min, int32_t max) : min_(min), max_(max) {}
        // 解方程 a <= b * x <= c，并更新当前区间
        void Update(int64_t a, int64_t b, int64_t c)
        {
            if (b == 0) {
                if (a > 0 || c < 0) {
                    max_ = min_;
                }
                return;
            }
            if (b < 0) {
                std::swap(a, c);
            }

            // ceil
            int delta = ((a % b) != 0 && ((a > 0) == (b > 0))) ? 1 : 0;
            min_ = std::max(min_, static_cast<int32_t>(a / b + delta));
            // floor
            delta = (c % b != 0 && ((b > 0) != (c > 0))) ? 1 : 0;
            max_ = std::min(max_, static_cast<int32_t>(c / b - delta + 1));
        }
        int32_t min_{0};
        int32_t max_{0};
    };

    MeVector<VectorI> GetAllOffset() const
    {
        MeVector<VectorI> res;
        VectorI vec(0, 0);
        VectorI offset_cols = offset_col_ * cols_;
        for (int i = 0; i < static_cast<int>(rows_); ++i) {
            for (int j = 0; j < static_cast<int>(cols_); ++j) {
                res.emplace_back(vec);
                vec += offset_col_;
            }
            vec -= offset_cols;
            vec += offset_row_;
        }
        return res;
    }

private:
    uint32_t rows_{0};
    uint32_t cols_{0};
    Vector<int32_t> offset_row_;
    Vector<int32_t> offset_col_;
};

}  // namespace medb2

#endif  // MEDB_ARRAY_INFO_H
