/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_POLYGON_H
#define MEDB_POLYGON_H

#include <algorithm>
#include <cstring>
#include <memory>
#include "medb/allocator.h"
#include "medb/box.h"
#include "medb/point.h"
#include "medb/point_utils.h"
#include "medb/ring_utils.h"
namespace medb2 {
/**
 * @brief 描述一个2D空间的多边形, 以自接触形式表示,
 * 可以外圈顺时针，内圈逆时针；也可以外圈逆时针，内圈顺时针。但是不可内外圈同向，否则结果不可预知
 *
 * @tparam CoordType 坐标类型，参考 Point
 */
template <class CoordType>
class Polygon {
public:
    Polygon()
    {
        point_data_ = nullptr;
        point_size_ = 0;
    }

    /**
     * @brief 构造函数，通过点的数组以及点的个数构造
     *
     * @param point_data 点集数组指针
     * @param point_size 数组的大小，也就是点的个数
     */
    Polygon(const Point<CoordType> *point_data, uint32_t point_size) : Polygon(point_data, point_size, PointsFlag()) {}

    /**
     * @brief 构造函数,使用 vector 进行构造
     *
     * @param points point 的 vector 点集
     */
    explicit Polygon(const std::vector<Point<CoordType>> &points)
        : Polygon(points.data(), static_cast<uint32_t>(points.size()))
    {
    }

    /**
     * @brief 构造函数，通过点的数组以及点的个数构造
     *
     * @param point_data 点集数组指针
     * @param point_size 数组的大小，也就是点的个数
     * @param points_flag 点集特征，包含压缩方式以及 manhattan 类型
     */
    Polygon(const Point<CoordType> *point_data, uint32_t point_size, const PointsFlag &points_flag)
    {
        Point<CoordType> *dup_points = new Point<CoordType>[point_size];
        std::copy(point_data, point_data + point_size, dup_points);
        Init(dup_points, point_size, points_flag);
    }

    /**
     * @brief 构造函数，通过点的数组以及点的个数构造
     *
     * @param point_data 点集数组指针，Polygon 会接管这个指针的管理权
     * @param point_size 数组的大小，也就是点的个数
     * @param points_flag 点集特征，包含点集特征、压缩方式以及 manhattan 类型
     */
    Polygon(std::unique_ptr<Point<CoordType>[]> point_data, uint32_t point_size, const PointsFlag &points_flag)
    {
        Point<CoordType> *points = point_data.release();
        Init(points, point_size, points_flag);
    }

    /**
     * @brief 构造函数,通过 vector 构造
     *
     * @param points vector 的 point 点集
     * @param points_flag points 点集数据特征
     */
    Polygon(const std::vector<Point<CoordType>> &points, const PointsFlag &points_flag)
        : Polygon(points.data(), static_cast<uint32_t>(points.size()), points_flag)
    {
    }

    Polygon(const Polygon &other)
        : bounding_box_(other.bounding_box_), point_size_(other.point_size_), flag_(other.flag_)
    {
        if (point_size_ == 0) {
            return;
        }
        point_data_ = new Point<CoordType>[other.point_size_];
        std::copy(other.point_data_, other.point_data_ + other.point_size_, point_data_);
    }

    Polygon(Polygon &&other) noexcept
        : bounding_box_(other.bounding_box_),
          point_data_(other.point_data_),
          point_size_(other.point_size_),
          flag_(other.flag_)
    {
        other.point_data_ = nullptr;
        other.point_size_ = 0;
        other.flag_ = PointsFlag();
        other.bounding_box_ = Box<CoordType>();
    }

    // 析构函数
    ~Polygon() { Clear(); }

    // 符号重载
    Polygon<CoordType> &operator=(const Polygon<CoordType> &other)
    {
        if (this != &other) {
            delete[] point_data_;
            point_data_ = nullptr;
            point_size_ = other.point_size_;
            if (point_size_ > 0) {
                point_data_ = new Point<CoordType>[other.point_size_];
                std::copy(other.point_data_, other.point_data_ + other.point_size_, point_data_);
            }
            flag_ = other.flag_;
            bounding_box_ = other.bounding_box_;
        }
        return *this;
    }

    Polygon<CoordType> &operator=(Polygon<CoordType> &&other) noexcept
    {
        if (this != &other) {
            delete[] point_data_;
            point_data_ = other.point_data_;
            point_size_ = other.point_size_;
            flag_ = other.flag_;
            bounding_box_ = other.bounding_box_;
            other.point_data_ = nullptr;
            other.point_size_ = 0;
        }
        return *this;
    }

    bool operator==(const Polygon<CoordType> &other) const
    {
        if (this == &other) {
            return true;
        }
        if (PointSize() != other.PointSize() || flag_ != other.flag_) {
            return false;
        }
        if (PointSize() == 0) {
            return true;
        }
        return std::memcmp(PointData(), other.PointData(), PointSize() * sizeof(Point<CoordType>)) == 0;
    }

    bool operator!=(const Polygon<CoordType> &other) const { return !(*this == other); }

    /**
     * @brief 获取 polygon 的实际数据，若压缩则返回压缩数据
     *
     * @return 点集的数据指针，可通过 PointSize() 函数获取点的个数进行遍历
     */
    const Point<CoordType> *PointData() const
    {
        if (Empty()) {
            return nullptr;
        }
        return point_data_;
    }

    /**
     * @brief 获取polygon的真实点集，若压缩则解压后返回
     *
     * @return 返回 std::vector 类型点的集合
     */
    std::vector<Point<CoordType>> Points() const
    {
        std::vector<Point<CoordType>> ret;
        if (flag_.compress_type != kNoCompress) {
            DecompressManhattanPoints(point_data_, point_size_, flag_.compress_type, ret);
        } else {
            ret.insert(ret.end(), point_data_, point_data_ + point_size_);
        }
        return ret;
    }

    /**
     * @brief 设置 polygon 的点集，若已存在数据将会把数据清空再进行设置，当 point_data 为空或 point_size 为 0 时仅清空
     * Polygon
     *
     * @param point_data 点数据的数组指针
     * @param point_size 点的个数
     */
    void SetPoints(const Point<CoordType> *point_data, uint32_t point_size)
    {
        SetPoints(point_data, point_size, PointsFlag());
    }

    /**
     * @brief 使用 vector 容器设置 polygon 的点集，若已存在数据将会把数据清空再进行设置
     *
     * @param points 点数据的 vector 容器
     */
    void SetPoints(const std::vector<Point<CoordType>> &points)
    {
        SetPoints(points.data(), static_cast<uint32_t>(points.size()));
    }

    /**
     * @brief 设置 polygon 的点集，若已存在数据将会把数据清空再进行设置
     *
     * @param point_data 点数据的数组指针
     * @param point_size 点的个数
     * @param flag 点集的特征，包含压缩方式以及 manhattan 类型
     */
    void SetPoints(const Point<CoordType> *point_data, uint32_t point_size, const PointsFlag &flag)
    {
        Clear();
        if (point_size == 0) {
            return;
        }
        Point<CoordType> *data = new Point<CoordType>[point_size];
        std::copy(point_data, point_data + point_size, data);
        Init(data, point_size, flag);
    }

    /**
     * @brief 使用 vector 容器设置 polygon 的点集，若已存在数据将会把数据清空再进行设置
     *
     * @param points 点数据的 vector 容器
     * @param flag 点集的特征，包含压缩方式以及 manhattan 类型
     */
    void SetPoints(const std::vector<Point<CoordType>> &points, const PointsFlag &flag)
    {
        SetPoints(points.data(), points.size(), flag);
    }

    /**
     * @brief 获取 polygon 的包围盒
     *
     */
    const Box<CoordType> &BoundingBox() const
    {
        if (Empty()) {
            bounding_box_ = Box<CoordType>();
        } else if (bounding_box_.Empty()) {
            bounding_box_ = GetBoundingBox(point_data_, point_size_);
        }
        return bounding_box_;
    }

    /**
     * @brief 判断当前 polygon 是否为空
     *
     * @return 为空则返回 true，否则返回 false
     */
    bool Empty() const { return point_size_ == 0; }

    /**
     * @brief 清空当前的 polygon，若需再使用当前 polygon 可调用 SetPoints 函数
     */
    void Clear()
    {
        if (point_data_ != nullptr) {
            delete[] point_data_;
            point_data_ = nullptr;
        }
        point_size_ = 0;
        flag_ = PointsFlag();
        bounding_box_.Clear();
    }

    /**
     * @brief 获取当前 polygon 点的存储个数,若压缩则是压缩点个数
     *
     * @return 返回 polygon 点的存储个数
     */
    uint32_t PointSize() const { return point_size_; }

    /**
     * @brief 获取当前 polygon 点的实际个数,若压缩则是解压后的点数
     *
     * @return 返回 polygon 点的实际个数
     */
    uint32_t RealPointSize() const
    {
        return Flag().compress_type != kNoCompress ? 2 * point_size_
                                                   : point_size_;  // 2 表示解压缩后点的个数是压缩时的两倍
    }

    /**
     * @brief 获取 points 的特征数据，包含Manhattan和压缩类型
     *
     * @return 返回 points 的特征
     */
    const PointsFlag &Flag() const { return flag_; }

    /**
     * @brief 对当前 polygon 进行变换操作
     *
     * @param trans 需要做的变换,包含1.镜像；2.缩放；3.旋转；4.位移
     * @return 改变当前 polygon 数据,返回自身
     */
    template <class TransType>
    Polygon<CoordType> &Transform(const TransType &trans)
    {
        for (int i = 0; i < static_cast<int>(point_size_); ++i) {
            trans.Transform(point_data_[i]);
        }

        if constexpr (std::is_same_v<TransType, SimpleTransformation>) {
            if (!bounding_box_.Empty()) {
                bounding_box_.Transform(trans);
            }
            return *this;
        }
        bounding_box_.Clear();

        if (trans.Magnification() < 0) {
            std::reverse(point_data_, point_data_ + point_size_);
        }

        if (flag_.manhattan_type != ShapeManhattanType::kManhattan) {
            if (flag_.manhattan_type == ShapeManhattanType::kOctangular &&
                !DoubleEqual(fabs(trans.Magnification()), 1.0)) {
                flag_.manhattan_type = GetPointsType(point_data_, point_size_);
            }
            return *this;
        }
        // 如果变换存在镜像分量或者旋转角度为90,270,且是压缩存储时,变换后压缩类型会发生改变
        // 如果两种变换组合,则压缩类型又不会改变
        bool degree_flag = (trans.Rotation() == kRotation90) || (trans.Rotation() == kRotation270);
        bool magn_flag = trans.Magnification() < 0;
        if (degree_flag != magn_flag) {
            if (flag_.compress_type == kCompressH) {
                flag_.compress_type = kCompressV;
            } else if (flag_.compress_type == kCompressV) {
                flag_.compress_type = kCompressH;
            }
        }
        return *this;
    }

    /**
     * @brief 对当前 polygon 进行复制,对副本进行变换操作
     *
     * @param trans 需要做的变换,包含1.镜像；2.缩放；3.旋转；4.位移
     * @return 改变副本 polygon 数据,返回副本变换后的 polygon
     */
    template <class TransType>
    Polygon<CoordType> Transformed(const TransType &trans) const
    {
        Polygon<CoordType> polygon_ret(*this);
        polygon_ret.Transform(trans);
        return polygon_ret;
    }

    /**
     * @brief 对当前 polygon 求面积，返回值大于等于0
     *
     * @return 返回求得的面积
     */
    double Area() const { return medb2::Area(point_data_, point_size_, flag_.compress_type); }

    // debug
    std::string ToString() const
    {
        std::string text = "{\n";
        for (int i = 0; i < static_cast<int>(point_size_); ++i) {
            text += point_data_[i].ToString();
            text += ",";
        }
        text += "\n}\n";
        return text;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    /**
     * @brief 初始化 polygon
     *
     * @param points 初始化点集的指针,若不使用(全是重复点,manhattan类型需要压缩等情况)则通过内部释放,
     * 否则直接给数据成员使用,通过析构函数释放
     * @param size 点集的个数
     * @param flag 压缩方式以及Manhattan类型
     */
    void Init(Point<CoordType> *points, uint32_t size, const PointsFlag &flag)
    {
        flag_.has_duplicate_or_collinear = false;
        if (flag.compress_type == kNoCompress && flag.has_duplicate_or_collinear) {
            RemoveDuplicateAndCollinearPoint(points, size);
            if (size < kRingMinPointCount) {
                point_size_ = 0;
                delete[] points;
                return;
            }
        }
        flag_.manhattan_type = flag.manhattan_type;
        if (flag.compress_type != kNoCompress) {
            flag_.manhattan_type = ShapeManhattanType::kManhattan;
        }

        if (flag_.manhattan_type == ShapeManhattanType::kUnknown) {
            flag_.manhattan_type = GetPointsType(points, size);
        }

        // 若是 Manhattan，则进行压缩
        if (flag_.manhattan_type == ShapeManhattanType::kManhattan && flag.compress_type == kNoCompress) {
            // 判断第一个点和第二个点的位置关系，确定压缩方式
            ManhattanCompressType compress_type = CoordEqual(points[0].Y(), points[1].Y()) ? kCompressH : kCompressV;
            flag_.compress_type = compress_type;
            point_size_ = size / 2;  // 2 表示压缩点个数是点个数的一半
            point_data_ = new Point<CoordType>[point_size_];
            CompressManhattanPoints<CoordType>(points, size, point_data_);
            delete[] points;
        } else {
            point_size_ = size;
            point_data_ = points;
            flag_.compress_type = flag.compress_type;
        }
    }

private:
    mutable Box<CoordType> bounding_box_{Box<CoordType>()};  // 缓存当前 polygon 的 bbox
    Point<CoordType> *point_data_{nullptr};                  // polygon 点集起始点的指针
    uint32_t point_size_{0};                                 // polygon 点集的个数
    PointsFlag flag_;                                        // polygon 点集的特征信息,压缩方式以及 manhattan 类型
};

using PolygonI = Polygon<int32_t>;
using PolygonD = Polygon<double>;
}  // namespace medb2

#endif  // MEDB_POLYGON_H
