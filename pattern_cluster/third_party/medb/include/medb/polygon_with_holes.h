/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_POLYGON_WITH_HOLES_H
#define MEDB_POLYGON_WITH_HOLES_H

#include "medb/box.h"
#include "medb/enums.h"
#include "medb/geometry_data.h"
#include "medb/point_utils.h"
#include "medb/ring.h"
#include "medb/ring_utils.h"
#include "medb/vector_utils.h"

namespace medb2 {

/**
 * @brief 描述一个2D空间的多边形
 *
 * @tparam CoordType 坐标类型，参考 Point
 */
template <class CoordType>
class PolygonWithHoles {
public:
    // 默认构造函数，无点
    PolygonWithHoles() {}

    // 析构函数
    ~PolygonWithHoles() = default;

    /**
     * @brief 构造函数，传入一个代表外轮廓的Ring的对象左值引用，构造出不带hole多边形，入参数据拷贝至polygon成员
     *
     * @param hull 输入一个Ring对象的对象左值引用
     */
    explicit PolygonWithHoles(const Ring<CoordType> &hull) { SetHull(hull); }

    /**
     * @brief 构造函数，传入一个代表外轮廓的Ring的右值引用，构造出不带hole多边形，入参数据移动至polygon成员
     *
     * @param hull 输入一个Ring对象的右值引用
     */
    explicit PolygonWithHoles(Ring<CoordType> &&hull) { SetHull(std::move(hull)); }

    /**
     * @brief 构造函数，传入一个代表轮廓数组的Ring数组的对象左值引用，构造出可能带洞的多边形，入参数据拷贝至polygon成员
     *
     * @param rings 输入一个Ring数组对象的对象左值引用
     */
    explicit PolygonWithHoles(const MeVector<Ring<CoordType>> &rings) { SetRings(rings); }

    /**
     * @brief 构造函数，传入一个代表轮廓数组的Ring数组的右值引用，构造出可能带洞的多边形，入参数据移动至polygon成员
     *
     * @param rings 输入一个Ring数组对象的右值引用
     */
    explicit PolygonWithHoles(MeVector<Ring<CoordType>> &&rings) { SetRings(std::move(rings)); }

    /**
     * @brief 构造函数，用一个有序点集常量左值引用，构造一个PolygonWithHoles对象
     *
     * @param points 表示一条路径的有序点集（不可修改），路径开或闭均为合法（即首尾点是否重复）
     */
    explicit PolygonWithHoles(const std::vector<Point<CoordType>> &points)
    {
        std::vector<Point<CoordType>> dup_points(points);
        Init(dup_points, PointsFlag());
    }

    /**
     * @brief 构造函数，用一个有序点集右值引用，构造一个PolygonWithHoles对象
     *
     * @param points 表示一条路径的有序点集（可修改），路径开或闭均为合法（即首尾点是否重复）
     */
    explicit PolygonWithHoles(std::vector<Point<CoordType>> &&points)
    {
        std::vector<Point<CoordType>> dup_points(std::move(points));
        Init(dup_points, PointsFlag());
    }

    /**
     * @brief 构造函数，用一个有序点集常量左值引用，构造一个PolygonWithHoles对象
     *
     * @param points 表示一条路径的有序点集（不可修改），路径开或闭均为合法（即首尾点是否重复）
     * @param points_flag 表示 points 的特征的 points 的对象,至少需要包含 compress type
     */
    PolygonWithHoles(const std::vector<Point<CoordType>> &points, const PointsFlag &points_flag)
    {
        std::vector<Point<CoordType>> dup_points(points);
        Init(dup_points, points_flag);
    }

    /**
     * @brief 构造函数，用一个有序点集右值引用，构造一个PolygonWithHoles对象
     *
     * @param points 表示一条路径的有序点集（可修改），路径开或闭均为合法（即首尾点是否重复）
     * @param points_flag 表示 points 的特征,至少需要包含 compress type
     */
    PolygonWithHoles(std::vector<Point<CoordType>> &&points, PointsFlag points_flag)
    {
        std::vector<Point<CoordType>> dup_points(std::move(points));
        Init(dup_points, points_flag);
    }

    PolygonWithHoles(const PolygonWithHoles<CoordType> &) = default;
    PolygonWithHoles(PolygonWithHoles<CoordType> &&) = default;

    PolygonWithHoles<CoordType> &operator=(const PolygonWithHoles<CoordType> &) = default;
    PolygonWithHoles<CoordType> &operator=(PolygonWithHoles<CoordType> &&) = default;

    /**
     * @brief 判断 polygon 相等
     *
     */
    bool operator==(const PolygonWithHoles<CoordType> &poly_other) const
    {
        return (BoundingBox() == poly_other.BoundingBox()) && (rings_ == poly_other.rings_);
    }

    /**
     * @brief 判断 polygon 不等
     *
     */
    bool operator!=(const PolygonWithHoles<CoordType> &poly_other) const { return !operator==(poly_other); }

    /**
     * @brief 传入一个Ring对象的对象左值引用，复制其数据至PolygonWithHoles的外轮廓对象
     * （注意：SetHull，AddHole等PolygonWithHoles内部方法，均不检查及处理其空间几何关系，比如hull没有包围hole，
     * 或者hole之间有相交等，PolygonWithHoles仅作为数据容器使用，空间几何关系由外部输入控制，后面不再赘述）
     *
     * @param hull 输入一个Ring对象的对象左值引用
     */
    void SetHull(const Ring<CoordType> &hull)
    {
        if (&hull != &rings_[0]) {
            rings_[0] = hull;
            bounding_box_ = Box<CoordType>();
            manhattan_type_ = ShapeManhattanType::kUnknown;
        }
    }

    /**
     * @brief 传入一个Ring对象的右值引用，移动其数据至PolygonWithHoles的外轮廓对象（不检查及处理空间几何关系）
     *
     * @param hull 输入一个Ring对象的对象右值引用
     */
    void SetHull(Ring<CoordType> &&hull)
    {
        if (&hull != &rings_[0]) {
            rings_[0] = std::move(hull);
            bounding_box_ = Box<CoordType>();
            manhattan_type_ = ShapeManhattanType::kUnknown;
        }
    }

    /**
     * @brief
     * 传入一个Ring对象的对象左值引用，使polygon增加一个内轮廓，复制其数据至该内轮廓对象（不检查及处理空间几何关系）
     *
     * @param hole 输入一个Ring对象的对象左值引用
     */
    void AddHole(const Ring<CoordType> &hole)
    {
        rings_.push_back(hole);
        manhattan_type_ = ShapeManhattanType::kUnknown;
    }

    /**
     * @brief 传入一个Ring对象的右值引用，使polygon增加一个内轮廓，移动其数据至该内轮廓对象（不检查及处理空间几何关系）
     *
     * @param hole 输入一个Ring对象的右值引用
     */
    void AddHole(Ring<CoordType> &&hole)
    {
        rings_.emplace_back(hole);
        manhattan_type_ = ShapeManhattanType::kUnknown;
    }

    /**
     * @brief
     * 传入一个Ring数组对象的对象左值引用，以设置polygon轮廓数据，复制数据至polygon内部（不检查及处理空间几何关系）
     *
     * @param rings 输入一个Ring对象集合的对象左值引用
     */
    void SetRings(const MeVector<Ring<CoordType>> &rings)
    {
        if (&rings != &rings_) {
            rings_ = rings;
            bounding_box_ = Box<CoordType>();
            manhattan_type_ = ShapeManhattanType::kUnknown;
        }
    }

    /**
     * @brief 传入一个Ring数组对象的右值引用，设置polygon轮廓数据，复制数据至polygon内部（不检查及处理空间几何关系）
     *
     * @param rings 输入一个Ring对象集合的对象右值引用
     */
    void SetRings(MeVector<Ring<CoordType>> &&rings)
    {
        if (&rings != &rings_) {
            rings_ = std::move(rings);
            bounding_box_ = Box<CoordType>();
            manhattan_type_ = ShapeManhattanType::kUnknown;
        }
    }

    /**
     * @brief 获取polygon的外轮廓对象左值引用（只读）
     *
     * @return const Ring<CoordType>& 返回表示polygon的外轮廓Ring的对象左值引用
     */
    const Ring<CoordType> &Hull() const { return rings_[0]; }

    /**
     * @brief 获取polygon的内轮廓数量（只读）
     *
     * @return size_t 返回polygon的内轮廓（洞）的数量
     */
    size_t HoleCount() const { return rings_.size() - 1; }

    /**
     * @brief
     * 获取指定index的polygon的内轮廓的引用，如果index大于等于实际存在的内轮廓的数量，则返回一个空的Ring的引用（只读）
     *
     * @param hole_idx 输入一个表示内轮廓index的无符号整形
     * @return const Ring<CoordType>& 返回polygon的内轮廓（洞）的数量
     */
    const Ring<CoordType> &Hole(size_t hole_idx) const
    {
        static Ring<CoordType> err_ring;
        if (hole_idx + 1 < rings_.size()) {
            return rings_[hole_idx + 1];
        } else {
            return err_ring;
        }
    }

    /**
     * @brief 获取polygon的轮廓数组的引用（只读）
     *
     * @return const MeVector<Ring<CoordType>>& 返回polygon的轮廓数组的对象左值引用
     */
    const MeVector<Ring<CoordType>> &Rings() const { return rings_; }

    /**
     * @brief 清理polygon的数据，重置成无参构造的状态
     *
     */
    void Clear()
    {
        rings_.clear();
        rings_.push_back(Ring<CoordType>());
        bounding_box_ = Box<CoordType>();
        manhattan_type_ = ShapeManhattanType::kUnknown;
    }

    /**
     * @brief 判断polygon是否是一个没有数据的空的polygon
     *
     * @return 空polygon返回true，否则返回false
     */
    bool Empty() const { return Hull().Points().empty() && HoleCount() == 0; }

    /**
     * @brief 将PolygonWithHoles的顶点数据整理成一个PolygonData，并以对象形式移交给外部，自身数据清空
     *
     * @return PolygonData<CoordType> 返回包含polygon顶点数据的PolygonData的对象
     */
    PolygonData<CoordType> TakeData()
    {
        PolygonData<CoordType> poly_data;
        for (auto &ring : rings_) {
            if (!ring.is_proxy_) {
                poly_data.emplace_back(std::move(ring.TakeData()));
            }
        }
        Clear();
        return poly_data;
    }

    /**
     * @brief 返回PolygonWithHoles的外包围盒成员对象的对象左值引用
     *
     * @return const Box<CoordType>& 返回PolygonWithHoles的外包围盒的对象左值引用
     */
    const Box<CoordType> &BoundingBox() const
    {
        if (bounding_box_.Empty()) {
            bounding_box_ = rings_[0].BoundingBox();
        }
        return bounding_box_;
    }

    /**
     * @brief
     * 返回PolygonWithHoles的Manhattan类型，如果所有轮廓均为kManhattan类型，则polygon为kManhattan类型，如果除kManhattan以外
     * 仅存在kHalfManhattan类型，则为kHalfManhattan，否则为kNoneManhattan（关于Manhattan类型的解释，详见geometry_utils的GetManhattanType描述）
     *
     * @return ShapeManhattanType 返回PolygonWithHoles的ManhattanType
     */
    ShapeManhattanType ManhattanType() const
    {
        if (Empty()) {
            return ShapeManhattanType::kUnknown;
        }
        if (manhattan_type_ == ShapeManhattanType::kUnknown) {
            manhattan_type_ = GetManhattanType(rings_);
        }
        return manhattan_type_;
    }

    /**
     * @brief 根据入参trans，原地变换PolygonWithHoles自身的几何数据，并返回自身的引用
     *
     * @tparam TransType 参数的类型
     * @param trans 传入一个用于变换该PolygonWithHoles的Transformation
     * @return PolygonWithHoles<CoordType>& 返回自身的引用
     */
    template <class TransType>
    PolygonWithHoles<CoordType> &Transform(const TransType &trans)
    {
        for (auto &ring : rings_) {
            ring.Transform(trans);
        }
        bounding_box_ = Box<CoordType>();
        return *this;
    }
    /**
     * @brief 根据入参trans，产生一个经入参变换之后的polygon副本对象，并返回。原polygon不发生变换
     *
     * @tparam TransType 参数的类型
     * @param trans 传入一个用于变换该PolygonWithHoles的Transformation
     * @return PolygonWithHoles<CoordType> 返回polygon经trans变换之后的副本对象
     */
    template <class TransType>
    PolygonWithHoles<CoordType> Transformed(const TransType &trans) const
    {
        PolygonWithHoles<CoordType> poly_ret(*this);
        poly_ret.Transform(trans);
        return poly_ret;
    }

    /**
     * @brief 获取自接触形式的点集（将hull和holes融合到一个点集中，即版图中表示形式）
     *
     * @return 融合后的自接触点集
     */
    std::vector<Point<CoordType>> GetMergedPoints()
    {
        if (rings_.size() == 1) {
            return rings_[0].Points();
        }
        std::vector<Point<CoordType>> result;
        MergeHoles(rings_, result);
        return result;
    }

    /**
     * @brief 将PolygonWithHoles的顶点数据转换成格式化字符串描述。
     *
     * @return std::string 返回描述polygon点数据的格式化字符串描述
     */
    std::string ToString() const
    {
        std::string text = "{\n";
        for (const auto &ring : rings_) {
            text += ring.ToString();
            text += ",\n";
        }
        text += "}\n";
        return text;
    }

private:
    void Init(std::vector<Point<CoordType>> &points, const PointsFlag &points_flag)
    {
        if (points_flag.compress_type == kNoCompress) {
            RemoveDuplicateAndCollinearPoint(points);
            if (points.size() < kRingMinPointCount) {
                return;
            }
        }

        PointsFlag flag;
        flag.manhattan_type = points_flag.manhattan_type;
        // 若为压缩点集,且未知图形类型
        if (points_flag.compress_type != kNoCompress) {
            flag.manhattan_type = ShapeManhattanType::kManhattan;
        }
        // 若未知图形形状，获取图形的形状
        if (flag.manhattan_type == ShapeManhattanType::kUnknown) {
            flag.manhattan_type = GetPointsType(points.data(), static_cast<uint32_t>(points.size()));
        }
        MeVector<Ring<CoordType>> split_result;
        // 若是压缩形式，则解压
        if (points_flag.compress_type != kNoCompress) {
            std::vector<Point<CoordType>> decompress_points;
            DecompressManhattanPoints<CoordType>(points, points_flag.compress_type, decompress_points);
            // 转换为 bopo 的 polygon，若有洞则拆分，没有就不变
            SplitHoles(decompress_points, flag.manhattan_type, split_result);
        } else {
            SplitHoles(points, flag.manhattan_type, split_result);
        }
        if (split_result.empty()) {
            // 若是 manhattan 且没有压缩，则进行压缩
            if (flag.manhattan_type == ShapeManhattanType::kManhattan && points_flag.compress_type == kNoCompress) {
                std::vector<Point<CoordType>> compress_points;
                // 判断第一个点和第二个点的位置关系，确定压缩方式
                ManhattanCompressType compress_type = points[0].Y() == points[1].Y() ? kCompressH : kCompressV;
                flag.compress_type = compress_type;
                CompressManhattanPoints<CoordType>(points, compress_points);
                // 通过获取到的正确的 points 点集特征，构造 Ring 来构造 PolygonWithHoles
                SetHull(Ring<CoordType>(std::move(compress_points), flag));
            } else {
                flag.compress_type = points_flag.compress_type;
                SetHull(Ring<CoordType>(std::move(points), flag));
            }
        } else {
            // 有洞，且是 manhattan 图形，压缩每一个 Ring
            if (flag.manhattan_type == ShapeManhattanType::kManhattan) {
                MeVector<Ring<CoordType>> compress_split;
                CompressManhattanRings<CoordType>(split_result, compress_split);
                SetRings(std::move(compress_split));
            } else {
                SetRings(std::move(split_result));
            }
        }
    }

private:
    MeVector<Ring<CoordType>> rings_{Ring<CoordType>()};
    mutable Box<CoordType> bounding_box_{Box<CoordType>()};
    mutable ShapeManhattanType manhattan_type_{ShapeManhattanType::kUnknown};
};

using PolygonWithHolesI = PolygonWithHoles<int32_t>;
using PolygonWithHolesD = PolygonWithHoles<double>;

}  // namespace medb2

#endif
