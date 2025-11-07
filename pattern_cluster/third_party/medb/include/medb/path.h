
/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_PATH_H
#define MEDB_PATH_H
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <utility>
#include <vector>
#include "medb/allocator.h"
#include "medb/point.h"
#include "medb/point_utils.h"
#include "medb/polygon.h"
#include "medb/vector_utils.h"

namespace medb2 {

/**
 * @brief Path表示一个路径。用所经过的点的集合来表示，并带有宽度、起始扩展长度和结束扩展长度属性。
 * 起始扩展长度是指path开始端的线往外的延伸长度，结束扩展长度是指path末端的线往外的延伸长度。
 * 反向共线:
 * 如{{0,0},{10,0},{5,0}}情况,实际场景中不存在这种情况,且竞品对这种情况的处理也存在问题,所以我们的不支持反向共线情况
 */
template <class CoordType>
class Path {
public:
    Path() {}

    ~Path() { Clear(); }

    /**
     * @brief 带参构造
     *
     * @param point_data 点集数组指针
     * @param point_size 数组大小
     * @param width Path的宽度
     * @param begin_extend 起始扩展长度
     * @param end_extend 结束扩展长度
     */
    Path(const Point<CoordType> *point_data, uint32_t point_size, CoordType width, CoordType begin_extend,
         CoordType end_extend)
    {
        if (point_size > 0) {
            Point<CoordType> *dup_points = new Point<CoordType>[point_size];
            std::copy(point_data, point_data + point_size, dup_points);
            FilterPath(dup_points, point_size);
            point_data_ = dup_points;
            point_size_ = point_size;

            width_ = width;
            begin_extend_ = begin_extend;
            end_extend_ = end_extend;
        }
    }

    /**
     * @brief vector 构造 path 对象
     *
     * @param points vector 容器,存储点集
     * @param width Path的宽度
     * @param begin_extend 起始扩展长度
     * @param end_extend 结束扩展长度
     */
    Path(const std::vector<Point<CoordType>> &points, CoordType width, CoordType begin_extend, CoordType end_extend)
        : Path(points.data(), static_cast<uint32_t>(points.size()), width, begin_extend, end_extend)
    {
    }

    Path(std::vector<Point<CoordType>> &&points, CoordType width, CoordType begin_extend, CoordType end_extend)
        : Path(points.data(), static_cast<uint32_t>(points.size()), width, begin_extend, end_extend)
    {
        points.clear();
    }

    Path(const Path<CoordType> &other)
        : point_size_(other.point_size_),
          width_(other.width_),
          begin_extend_(other.begin_extend_),
          end_extend_(other.end_extend_)
    {
        if (point_size_ > 0) {
            point_data_ = new Point<CoordType>[other.point_size_];
            std::copy(other.point_data_, other.point_data_ + other.point_size_, point_data_);
        }
        polygon_.Clear();
    }

    Path(Path<CoordType> &&other) noexcept { *this = std::move(other); }

    Path<CoordType> &operator=(const Path<CoordType> &other)
    {
        if (this != &other) {
            delete[] point_data_;
            point_data_ = nullptr;
            point_size_ = other.point_size_;
            if (point_size_ > 0) {
                point_data_ = new Point<CoordType>[other.point_size_];
                std::copy(other.point_data_, other.point_data_ + other.point_size_, point_data_);
            }
            width_ = other.width_;
            begin_extend_ = other.begin_extend_;
            end_extend_ = other.end_extend_;
            polygon_.Clear();
        }
        return *this;
    }

    Path<CoordType> &operator=(Path<CoordType> &&other) noexcept
    {
        if (this != &other) {
            delete[] point_data_;
            point_data_ = other.point_data_;
            point_size_ = other.point_size_;
            width_ = other.width_;
            begin_extend_ = other.begin_extend_;
            end_extend_ = other.end_extend_;
            polygon_.Clear();
            other.point_data_ = nullptr;
            other.point_size_ = 0;
            other.width_ = 0;
            other.begin_extend_ = 0;
            other.end_extend_ = 0;
            other.polygon_.Clear();
        }
        return *this;
    }

    /**
     * @brief 设置当前Path的所有点坐标。
     * @param points vector 容器, 要设置的Path所有点坐标。
     */
    void SetPoints(const std::vector<Point<CoordType>> &points)
    {
        SetPoints(points.data(), static_cast<uint32_t>(points.size()));
    }

    /**
     * @brief 设置当前path的点集，当 point_data 为空或 point_size 为 0 时仅清空 Path
     *
     * @param point_data 点集数组指针
     * @param point_size 数组大小
     */
    void SetPoints(const Point<CoordType> *point_data, uint32_t point_size)
    {
        if (point_data_ != nullptr) {
            delete[] point_data_;
            point_data_ = nullptr;
        }
        point_size_ = 0;
        polygon_.Clear();
        if (point_data == nullptr || point_size == 0) {
            return;
        }
        Point<CoordType> *data = new Point<CoordType>[point_size];
        std::copy(point_data, point_data + point_size, data);
        FilterPath(data, point_size);
        point_data_ = data;
        point_size_ = point_size;
    }

    /**
     * @brief 设置当前Path的宽度。
     * @param width Path的宽度。
     */
    void SetWidth(CoordType width)
    {
        width_ = width;
        polygon_.Clear();
    }

    /**
     * @brief 设置当前Path的起始扩展长度和结束扩展长度。
     * @param begin_extend 起始扩展长度。
     * @param end_extend 结束扩展长度。
     */
    void SetExtend(CoordType begin_extend, CoordType end_extend)
    {
        begin_extend_ = begin_extend;
        end_extend_ = end_extend;
        polygon_.Clear();
    }

    /**
     * @brief 判断 Path 是否为空
     *
     * @return 空返回 true，非空返回 false
     */
    bool Empty() const
    {
        if (point_size_ == 0) {
            return true;
        }

        if (point_size_ == 1) {
            return width_ <= 1 || (begin_extend_ == 0 && end_extend_ == 0);
        }

        return width_ <= 1;
    }

    /**
     * @brief 判断另外一个Path对象b和当前Path对象是否相等。
     * @param p 需要和当前Path做等于比较操作的另外一个Path对象。
     * @return 如果b和当前Path相等则返回true，否则返回false
     */
    bool operator==(const Path<CoordType> &p) const
    {
        if (width_ != p.width_ || begin_extend_ != p.begin_extend_ || end_extend_ != p.end_extend_ ||
            point_size_ != p.point_size_) {
            return false;
        }
        if (point_data_ == nullptr && p.point_data_ == nullptr) {  // 两个空Path认为相等
            return true;
        } else if (point_data_ == nullptr || p.point_data_ == nullptr) {
            return false;
        }

        return (std::memcmp(point_data_, p.point_data_, point_size_ * sizeof(Point<CoordType>)) == 0);
    }

    /**
     * @brief 判断另外一个Path对象b和当前Path对象是否不等。
     * @param p 需要和当前Path做不等于比较操作的另外一个Path对象。
     * @return 如果b和当前Path不相等则返回true，否则返回false
     */
    bool operator!=(const Path<CoordType> &p) const { return !(*this == p); }

    /**
     * @brief 判断另外一个Path对象b是否小于当前Path对象。
     * @param p 需要和当前Path做小于比较操作的另外一个Path对象。
     * @return 依次比较 width_, begin_extend_, end_extend, point_size, point_data_ 是否 <；
     * 如果发现中途有<情况成立，则为真，如果比较到points_的时候< 都不成立，则为假。
     */
    bool operator<(const Path<CoordType> &p) const
    {
        if (width_ != p.width_) {
            return width_ < p.width_;
        }

        if (begin_extend_ != p.begin_extend_) {
            return begin_extend_ < p.begin_extend_;
        }

        if (end_extend_ != p.end_extend_) {
            return end_extend_ < p.end_extend_;
        }

        if (point_size_ != p.point_size_) {
            return point_size_ < p.point_size_;
        }

        return std::lexicographical_compare(point_data_, point_data_ + std::min(point_size_, p.point_size_),
                                            p.point_data_, p.point_data_ + std::min(point_size_, p.point_size_));
    }
    /**
     * @brief 获取当前Path的所有点坐标。
     * @return 返回Path的所有点坐标。
     */
    std::vector<Point<CoordType>> Points() const
    {
        std::vector<Point<CoordType>> ret;
        ret.insert(ret.end(), point_data_, point_data_ + point_size_);
        return ret;
    }

    /**
     * @brief 获取当前 Path 的所有坐标, 配合 PointSize 使用
     *
     * @return 返回点集数组的指针
     */
    const Point<CoordType> *PointData() const { return point_data_; }

    /**
     * @brief 获取当前 Path 的坐标数组的大小
     *
     * @return 返回点集点的个数
     */
    uint32_t PointSize() const { return point_size_; }

    /**
     * @brief 获取当前Path的宽度。
     * @return 返回Path的宽度。
     */
    CoordType Width() const { return width_; }
    /**
     * @brief 获取当前Path的起始扩展长度。
     * @return 返回Path的起始扩展长度。
     */
    CoordType BeginExtend() const { return begin_extend_; }
    /**
     * @brief 获取当前Path的结束扩展长度。
     * @return 返回Path的结束扩展长度。
     */
    CoordType EndExtend() const { return end_extend_; }

    /**
     * @brief 将 Path 对象转换为 Polygon 对象
     * @return Polygon<CoordType> 返回转换后的 Polygon 对象
     */
    const Polygon<CoordType> &ToPolygon() const noexcept
    {
        UpdatePolygon();
        return polygon_;
    }

    /**
     * @brief 获取 path 的包围盒
     *
     */
    const Box<CoordType> &BoundingBox() const { return ToPolygon().BoundingBox(); }

    /**
     * @brief 根据Transform对象t对width、begin_extend和end_extend进行缩放，对points的变换可参考Transform。
     * 缩放的结果保存在当前Path对象中。
     * @param t 要变换的Transform对象，包含角度，位移和缩放。
     * @return 返回变换后的当前Path对象。
     */
    template <class TransType>
    Path<CoordType> &Transform(const TransType &t)
    {
        width_ = t.Scale(width_);
        begin_extend_ = t.Scale(begin_extend_);
        end_extend_ = t.Scale(end_extend_);
        for (int i = 0; i < static_cast<int>(point_size_); ++i) {
            t.Transform(point_data_[i]);
        }
        polygon_.Clear();
        return *this;
    }

    /**
     * @brief 根据Transform对象t对width、begin_extend和end_extend进行缩放，对points的变换可参考Transform。
     * 生成缩放后的Path，并返回该Path对象。
     * @param t 要变换的Transform对象，包含角度，位移和缩放。
     * @return 返回根据缩放生成的Path对象。
     */
    template <class TransType>
    Path<CoordType> Transformed(const TransType &t) const
    {
        Path<CoordType> path_ret(*this);
        path_ret.Transform(t);
        return path_ret;
    }

    /**
     * @brief 清空当前对象
     */
    void Clear()
    {
        if (point_data_ != nullptr) {
            delete[] point_data_;
            point_data_ = nullptr;
        }
        point_size_ = 0;
        width_ = 0;
        begin_extend_ = 0;
        end_extend_ = 0;
        polygon_.Clear();
    }

    /**
     * @brief 将Path转换成对应的字符串。
     * @return 返回Path转换成的字符串。
     */
    std::string ToString() const
    {
        std::string str = "{\n";
        for (uint32_t i = 0; i < point_size_; ++i) {
            str += point_data_[i].ToString();
            str += ",";
        }
        str += "\n}\n";
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    // 只有一个点的转换情况
    void ConvertSinglePoint(MeVector<Point<CoordType>> *points) const
    {
        Vector<CoordType> unit_normal_90(0, 1);
        Vector<CoordType> unit_normal_270(0, -1);
        Vector<CoordType> unit_v(1, 0);
        auto half_width = HalfWidth();

        // 转为 2 个点，实际效果为无点的polygon
        if (fabs(begin_extend_) < kDoubleEPS && fabs(end_extend_) < kDoubleEPS) {
            points->emplace_back(point_data_[0] + unit_normal_90 * half_width);
            points->emplace_back(point_data_[0] + unit_normal_270 * half_width);
        } else {
            points->emplace_back(point_data_[0] + unit_normal_90 * half_width + unit_v * (-begin_extend_));
            points->emplace_back(point_data_[0] + unit_normal_90 * half_width + unit_v * (end_extend_));
            points->emplace_back(point_data_[0] + unit_normal_270 * half_width + unit_v * (end_extend_));
            points->emplace_back(point_data_[0] + unit_normal_270 * half_width + unit_v * (-begin_extend_));
        }
    }

    // 转换第一个端点
    void ConvertFirstEndCap(MeVector<Point<CoordType>> *points90, MeVector<Point<CoordType>> *points270) const
    {
        auto temp = point_data_[1] - point_data_[0];
        Vector<double> v(temp.X(), temp.Y());
        auto unit_normal_90 = UnitNormal90(v);
        auto unit_normal_270 = -unit_normal_90;
        auto unit_v = Unit(v);
        auto half_width = HalfWidth();

        points90->emplace_back(point_data_[0] + (unit_normal_90 * half_width + unit_v * (-begin_extend_)));
        points270->emplace_back(point_data_[0] + (unit_normal_270 * half_width + unit_v * (-begin_extend_)));
    }

    // 转换中间的点
    void ConvertMiddlePoints(MeVector<Point<CoordType>> *points90, MeVector<Point<CoordType>> *points270) const
    {
        for (int i = 1; i < static_cast<int>(point_size_) - 1; i++) {
            ConvertOnePoint(point_data_[i - 1], point_data_[i], point_data_[i + 1], points90, points270);
        }
    }

    // 转换最后一个端点
    void ConvertLastEndCap(MeVector<Point<CoordType>> *points90, MeVector<Point<CoordType>> *points270) const
    {
        auto temp = point_data_[point_size_ - 1] - point_data_[point_size_ - 2];  // 1 找最后一个点，2 找倒数第二个点
        Vector<double> v(temp.X(), temp.Y());
        auto unit_normal_90 = UnitNormal90(v);
        auto unit_normal_270 = -unit_normal_90;
        auto unit_v = Unit(v);
        auto half_width = HalfWidth();

        points90->emplace_back(point_data_[point_size_ - 1] + (unit_normal_90 * half_width + unit_v * (end_extend_)));
        points270->emplace_back(point_data_[point_size_ - 1] + (unit_normal_270 * half_width + unit_v * (end_extend_)));
    }

    // 计算对称直角四边形的第四个点
    // A, B和C是已知的三个点，求D，其中AB长度和AC长度一致，角DBA和角DCA是直角
    Point<CoordType> CalcFourthPoint(const PointD &a, const Vector<double> &ab, const Vector<double> &ac) const
    {
        if (DoubleEqual(ab.X(), ac.X()) && DoubleEqual(ab.Y(), ac.Y())) {  // 同向共线
            return Point<CoordType>(a + ab);
        }
        if (DoubleEqual(ab.X(), -ac.X()) && DoubleEqual(ab.Y(), -ac.Y())) {  // 反向共线
            return Point<CoordType>(a);
        }
        if (DoubleEqual(DotProduct(ab, ac), 0)) {  // 垂直
            return Point<CoordType>(a + ab + ac);
        }

        // 对角线焦点为E
        auto ae = (ab + ac) / 2;  // 2 求平均

        // 因为对称性，对角线互相垂直，利用相似性可以推导出向量AD的计算公式
        auto ab_len = VectorLength(ab);
        auto ae_len = VectorLength(ae);
        auto ad = ae * ab_len * ab_len / (ae_len * ae_len);

        auto d = ad + a;

        return Point<CoordType>(d);
    }

    void ConvertOnePoint(const Point<CoordType> &pre, const Point<CoordType> &cur, const Point<CoordType> &nxt,
                         MeVector<Point<CoordType>> *out_forward, MeVector<Point<CoordType>> *out_backward) const
    {
        auto half_width = HalfWidth();

        Vector<double> pre_cur((cur - pre).X(), (cur - pre).Y());
        Vector<double> cur_nxt((nxt - cur).X(), (nxt - cur).Y());

        auto unit_normal_pre = UnitNormal90(pre_cur);
        auto unit_normal_nxt = UnitNormal90(cur_nxt);
        auto normal_pre = unit_normal_pre * half_width;
        auto normal_nxt = unit_normal_nxt * half_width;

        for (int i = 0; i < 2; i++) {  // 2 个方向计算
            auto &out = (i == 0) ? *out_forward : *out_backward;
            // 只有当锐角外延输出三个点，即法向量夹角在(180, 270)之间
            if (DoubleLess(CrossProduct(unit_normal_pre, unit_normal_nxt), 0) &&
                DoubleLess(DotProduct(unit_normal_pre, unit_normal_nxt), 0)) {
                // 反向延长做轴
                auto rev_ext_pre = Unit(pre_cur) * half_width;
                auto rev_ext_nxt = -Unit(cur_nxt) * half_width;

                auto first = CalcFourthPoint(Vector<double>(cur), normal_pre, rev_ext_nxt);
                auto second = CalcFourthPoint(Vector<double>(cur), rev_ext_nxt, rev_ext_pre);
                auto third = CalcFourthPoint(Vector<double>(cur), rev_ext_pre, normal_nxt);

                auto dot = fabs(DotProduct(Vector<double>(first - second), normal_pre));
                if (!(dot < kDoubleEPS)) {
                    out.emplace_back(first);
                }

                out.emplace_back(second);

                dot = fabs(DotProduct(Vector<double>(third - second), normal_pre));
                if (!(dot < kDoubleEPS)) {
                    out.emplace_back(third);
                }
            } else {
                out.emplace_back(CalcFourthPoint(Vector<double>(cur), normal_pre, normal_nxt));
            }

            normal_pre = -normal_pre;
            normal_nxt = -normal_nxt;
            std::swap(unit_normal_pre, unit_normal_nxt);
        }
    }

    double HalfWidth() const { return width_ / 2; }  // 2 表示求半宽，注意 width_ 为奇数时会进行取整

    void UpdatePolygon() const noexcept
    {
        if (!polygon_.Empty()) {
            return;
        }

        MeVector<Point<CoordType>> points;
        if (point_size_ == 0) {
            return;
        } else if (point_size_ == 1) {
            ConvertSinglePoint(&points);
        } else {
            MeVector<Point<CoordType>> points90;   // 90 度法线方向上的点集
            MeVector<Point<CoordType>> points270;  // 270 度法线方向上的点集
            points90.reserve(point_size_ * 3);
            points270.reserve(point_size_ * 3);
            ConvertFirstEndCap(&points90, &points270);
            if (point_size_ > 2) {  // 2 表示 path 的最小的点数
                ConvertMiddlePoints(&points90, &points270);
            }
            ConvertLastEndCap(&points90, &points270);
            points.reserve(points90.size() + points270.size());
            points.insert(points.end(), points90.begin(), points90.end());
            points.insert(points.end(), points270.rbegin(), points270.rend());
        }
        polygon_.SetPoints(points.data(), static_cast<uint32_t>(points.size()));
    }

    Point<CoordType> *point_data_{nullptr};
    uint32_t point_size_{0};
    CoordType width_{0};
    CoordType begin_extend_{0};
    CoordType end_extend_{0};
    mutable Polygon<CoordType> polygon_;
};

using PathI = Path<int32_t>;
using PathD = Path<double>;

}  // namespace medb2
#endif  // MEDB_PATH_H
