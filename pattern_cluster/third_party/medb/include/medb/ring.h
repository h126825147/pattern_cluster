/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_RING_H
#define MEDB_RING_H

#include "medb/allocator.h"
#include "medb/box.h"
#include "medb/geometry_data.h"
#include "medb/point.h"
#include "medb/point_utils.h"
#include "medb/transformation.h"

#include <algorithm>
#include <vector>

namespace medb2 {
template <class CoordType>
class PolygonWithHoles;

/**
 * @brief 描述一个2D空间的闭环曲线，顺时针是hull，逆时针是hole
 *
 * @tparam CoordType 坐标类型，参考 Point
 */
template <class CoordType>
class Ring {
public:
    friend class PolygonWithHoles<CoordType>;

    Ring() : is_proxy_(false) { ring_data_ = medb_new<std::vector<Point<CoordType>>>(); }

    // 析构函数
    ~Ring()
    {
        if (!is_proxy_ && ring_data_ != nullptr) {
            medb_delete(ring_data_);
            ring_data_ = nullptr;
        }
    }
    /**
     * @brief 自定义构造，创建空的点数组，并复制入参数据
     *
     * @param points 输入Point数组的对象引用
     */
    explicit Ring(const std::vector<Point<CoordType>> &points) { Init(points, PointsFlag()); }

    /**
     * @brief 自定义构造，创建空的点数组，并复制入参数据
     *
     * @param points 输入Point数组的对象引用
     * @param points_flag 描述 points 的特征的结构体,见 PointsFlag 的定义
     */
    explicit Ring(const std::vector<Point<CoordType>> &points, PointsFlag points_flag) { Init(points, points_flag); }

    /**
     * @brief 自定义构造，创建空的点数组，并移动入参数据至自身
     *
     * @param points 输入Point数组的右值引用
     */
    explicit Ring(std::vector<Point<CoordType>> &&points) { Init(std::move(points), PointsFlag()); }

    /**
     * @brief 自定义构造，创建空的点数组，并移动入参数据至自身
     *
     * @param points 输入Point数组的右值引用
     * @param points_flag 描述 points 的特征的结构体,见 PointsFlag 的定义
     */
    explicit Ring(std::vector<Point<CoordType>> &&points, PointsFlag points_flag)
    {
        Init(std::move(points), points_flag);
    }

    /**
     * @brief 自定义构造，代理模式,共享或接管数据
     *
     * @param ring_data point 数组的指针,传入用户方点集的内存地址,以代理模式访问或处理点集
     */
    explicit Ring(std::vector<Point<CoordType>> *ring_data) { Proxy(ring_data, PointsFlag()); }

    /**
     * @brief 自定义构造，代理模式,共享或接管数据
     *
     * @param ring_data point 数组的指针,传入用户方点集的内存地址,以代理模式访问或处理点集
     * @param points_flag 描述 points 特征的结构体,详见 PointsFlag 定义
     */
    explicit Ring(std::vector<Point<CoordType>> *ring_data, PointsFlag points_flag) { Proxy(ring_data, points_flag); }

    /**
     * @brief 复制构造，不会复制代理模式
     *
     * @param ring_other 输入被复制Ring的对象引用
     */
    Ring(const Ring<CoordType> &ring_other) : is_proxy_(false), flag_(ring_other.flag_)
    {
        ring_data_ = medb_new<std::vector<Point<CoordType>>>(*ring_other.ring_data_);
    }

    /**
     * @brief 移动构造，不会复制代理模式
     *
     * @param ring_other 输入被移动Ring的右值引用
     */

    Ring(Ring<CoordType> &&ring_other) : is_proxy_(false), flag_(ring_other.flag_)
    {
        if (ring_other.is_proxy_) {
            ring_data_ = medb_new<std::vector<Point<CoordType>>>(std::move(*ring_other.ring_data_));
        } else {
            ring_data_ = ring_other.ring_data_;
            ring_other.ring_data_ = nullptr;
        }
    }

    /**
     * @brief 复制赋值等号运算符，不会复制代理模式
     *
     * @param ring_other 输入被复制Ring的对象引用
     * @return Ring<CoordType>& 返回自身引用
     */
    Ring<CoordType> &operator=(const Ring<CoordType> &ring_other)
    {
        if (&ring_other != this) {
            if (ring_data_ == nullptr) {  // this 是一个被 move 的非代理对象，现在需要给它重新赋值
                ring_data_ = medb_new<std::vector<Point<CoordType>>>(*ring_other.ring_data_);
            } else {
                *ring_data_ = *ring_other.ring_data_;
            }

            flag_ = ring_other.flag_;
        }
        return *this;
    }

    /**
     * @brief 移动赋值等号运算符，不会复制代理模式
     *
     * @param ring_other 输入被移动Ring的右值引用
     * @return Ring<CoordType>& 返回自身引用
     */
    Ring<CoordType> &operator=(Ring<CoordType> &&ring_other)
    {
        if (&ring_other != this) {
            if (ring_data_ == nullptr) {  // this 是一个被 move 的非代理对象，现在需要给它重新赋值
                if (ring_other.is_proxy_) {
                    ring_data_ = medb_new<std::vector<Point<CoordType>>>(std::move(*ring_other.ring_data_));
                } else {
                    ring_data_ = ring_other.ring_data_;
                    ring_other.ring_data_ = nullptr;
                }
            } else {
                *ring_data_ = std::move(*ring_other.ring_data_);
            }

            flag_ = std::move(ring_other.flag_);
        }
        return *this;
    }

    /**
     * @brief 判断 ring 相等
     */
    bool operator==(const Ring<CoordType> &ring_other) const { return *ring_data_ == *(ring_other.ring_data_); }

    /**
     * @brief 判断 ring 不等
     *
     */
    bool operator!=(const Ring<CoordType> &ring_other) const { return !operator==(ring_other); }

    void SetPoints(const std::vector<Point<CoordType>> &points) { SetPoints(points, PointsFlag()); }

    void SetPoints(const std::vector<Point<CoordType>> &points, PointsFlag points_flag)
    {
        if (&points == ring_data_ || (points_flag.compress_type == kNoCompress && points.size() < kRingMinPointCount) ||
            (flag_.compress_type != kNoCompress && points.size() < kCompressRingMinPointCount)) {
            return;
        }
        (*ring_data_) = points;
        flag_ = points_flag;
    }

    /**
     * @brief 输入一个点数组，将其数据复制到Ring的内部成员中，即设置Ring的顶点数据
     *
     * @param points 输入Point数组的对象引用
     */
    void SetPoints(std::vector<Point<CoordType>> &&points) { SetPoints(std::move(points), PointsFlag()); }

    /**
     * @brief 输入一个点数组，将其数据复制到Ring的内部成员中，即设置Ring的顶点数据
     *
     * @param points 输入Point数组的对象引用
     * @param points_flag 表示 points 的特征,至少需要包含 compress type
     */
    void SetPoints(std::vector<Point<CoordType>> &&points, PointsFlag points_flag)
    {
        if (&points == ring_data_ || (points_flag.compress_type == kNoCompress && points.size() < kRingMinPointCount) ||
            (flag_.compress_type != kNoCompress && points.size() < kCompressRingMinPointCount)) {
            return;
        }
        (*ring_data_) = std::move(points);
        flag_ = points_flag;
    }

    /**
     * @brief 获取该Ring的顶点数量
     *
     * @return size_t 返回Ring的顶点数量,如果是压缩模式存储,则返回解压后的点数量.
     */
    size_t PointCount() const
    {
        if (flag_.compress_type == kNoCompress) {
            return (*ring_data_).size();
        } else {
            return (*ring_data_).size() * 2;  // 2 表示解压缩点是压缩时的两倍
        }
    }

    /**
     * @brief 获取 ring 中的原始存储形式的点集数据的左值常量引用(只读)
     *
     * @return ring 中代表点集的核心成员的引用
     */
    const std::vector<Point<CoordType>> &Raw() const { return *ring_data_; }

    /**
     * @brief 获取 ring 的实际顶点数组
     *
     * @return 实际的顶点数组(如果是压缩模式,则返回解压后的副本)
     */
    std::vector<Point<CoordType>> Points() const
    {
        if (flag_.compress_type == kNoCompress) {
            return *ring_data_;
        } else {
            std::vector<Point<CoordType>> ret;
            DecompressManhattanPoints<CoordType>(*ring_data_, flag_.compress_type, ret);
            return ret;
        }
    }

    /**
     * @brief 获取 ring 的数据特征,如压缩方式,manhattan类型
     *
     */
    const PointsFlag Flag() const { return flag_; }

    /**
     * @brief 获取Ring的外包围盒
     *
     * @return Ring的外包围盒对象
     */
    Box<CoordType> BoundingBox() const
    {
        if (ring_data_->empty()) {
            return Box<CoordType>();
        }
        CoordType xmin = std::numeric_limits<CoordType>::max();
        CoordType ymin = std::numeric_limits<CoordType>::max();
        CoordType xmax = std::numeric_limits<CoordType>::lowest();
        CoordType ymax = std::numeric_limits<CoordType>::lowest();
        for (auto pt = ring_data_->cbegin(); pt != ring_data_->cend(); ++pt) {
            if ((*pt).X() < xmin) {
                xmin = (*pt).X();
            }
            if ((*pt).Y() < ymin) {
                ymin = (*pt).Y();
            }
            if ((*pt).X() > xmax) {
                xmax = (*pt).X();
            }
            if ((*pt).Y() > ymax) {
                ymax = (*pt).Y();
            }
        }
        return Box<CoordType>(xmin, ymin, xmax, ymax);
    }

    /**
     * @brief 根据入参trans，原地变换Ring自身，并返回自身的应用
     *
     * @tparam TransType 参数的类型
     * @param trans 用于变换Ring的Transformation对象引用
     * @return Ring<CoordType>& 返回自身引用
     */
    template <class TransType>
    Ring<CoordType> &Transform(const TransType &trans)
    {
        for (auto &pt : (*ring_data_)) {
            trans.Transform(pt);
        }
        if (trans.Magnification() < 0) {
            // 修改为首尾点逆序,若压缩点集为两个点,初始点和倒数第二个点逆序情况异常
            std::reverse(ring_data_->begin(), ring_data_->end());
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
     * @brief 根据入参trans，产生一个经入参trans变换后的Ring的副本对象，并返回。Ring自身不发生变换
     *
     * @tparam TransType 参数的类型
     * @param trans 用于变换Ring的Transformation对象引用
     * @return Ring<CoordType> 返回变换后的ring的副本对象
     */
    template <class TransType>
    Ring<CoordType> Transformed(const TransType &trans) const
    {
        Ring<CoordType> ring_ret(*this);
        ring_ret.Transform(trans);
        return ring_ret;
    }

    /**
     * @brief 将Ring的顶点数据转换成一个RingData，并以对象形式移交给外部，自身数据清空
     *
     * @return RingData<CoordType> 返回包含ring顶点数据的RingData的对象
     */
    RingData<CoordType> TakeData()
    {
        if (flag_.compress_type != kNoCompress) {
            *ring_data_ = std::move(Points());
        }
        return std::move(*ring_data_);
    }

    /**
     * @brief 清空数据
     */
    void Clear()
    {
        if (ring_data_ != nullptr) {
            ring_data_->clear();
        } else {
            ring_data_ = medb_new<std::vector<Point<CoordType>>>();
            is_proxy_ = false;
        }
    }

    /**
     * @brief 将Ring的顶点数据内容，格式化成一个字符串,内容为空时为：{},内容非空时，如{{1,2},{3,4},{5,6},},
     *
     * @return std::string 返回Ring的顶点数据的格式化字符串
     */
    std::string ToString() const
    {
        std::string text = "{";
        for (const auto &pt : (*ring_data_)) {
            text += pt.ToString();
            text += ",";
        }
        text += "}";
        return text;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    template <class PointsType>
    void Init(PointsType &&points, const PointsFlag &points_flag)
    {
        ring_data_ = medb_new<std::vector<Point<CoordType>>>();
        SetPoints(std::forward<PointsType>(points), points_flag);
        is_proxy_ = false;
        flag_ = points_flag;
    }

    void Proxy(std::vector<Point<CoordType>> *ring_data, const PointsFlag &points_flag)
    {
        if (ring_data == nullptr || ring_data->size() < kRingMinPointCount) {
            ring_data_ = medb_new<std::vector<Point<CoordType>>>();
            is_proxy_ = false;
            flag_ = PointsFlag();
        } else {
            ring_data_ = ring_data;
            is_proxy_ = true;
            flag_ = points_flag;
        }
    }

private:
    std::vector<Point<CoordType>> *ring_data_{nullptr};
    // 表示是否为代理模式,代理模式即 ring_data_ 指向用户的数据,对象可以直接操作用户的数据
    bool is_proxy_{false};
    // 表示 ring_data_ 的一些数据特征,比如存储方式(压缩/非压缩),manhattan 类型(未知/任意/45/90)
    PointsFlag flag_;
};

using RingI = Ring<int32_t>;
using RingD = Ring<double>;
}  // namespace medb2

#endif