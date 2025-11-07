/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_TRANSFORMATION_H
#define MEDB_TRANSFORMATION_H

#include <cmath>
#include <string>
#include <variant>
#include "medb/base_utils.h"
#include "medb/const.h"
#include "medb/enums.h"
#include "medb/point.h"
#include "medb/vector_utils.h"

namespace medb2 {
template <class Integer_a, class Integer_b>
inline bool CheckIntegerAddOverflow(Integer_a a, Integer_b b)
{
    static_assert(std::is_integral_v<Integer_a> && std::is_integral_v<Integer_b>);
    if (a > 0 && b > 0) {
        return std::numeric_limits<Integer_a>::max() - a < b;
    }

    if (a < 0 && b < 0) {
        return std::numeric_limits<Integer_a>::min() - a > b;
    }

    return false;
}

class SimpleTransformation {
public:
    SimpleTransformation() = default;
    ~SimpleTransformation() = default;
    SimpleTransformation(const SimpleTransformation &) = default;
    SimpleTransformation &operator=(const SimpleTransformation &) = default;

    /**
     * @brief 带参构造函数
     *
     * @param translation 位移向量
     */
    explicit SimpleTransformation(const Vector<int32_t> &translation) : translation_(translation) {}

    /**
     * @brief 设置位移参数
     *
     * @param translation 位移向量
     */
    void Set(const Vector<int32_t> &translation) { translation_ = translation; }

    /**
     * @brief 对参数 d 进行缩放，缩放系数为 magnification_ 的绝对值；d
     * 的类型可以为非数值类型，当为非数值类型时，运算结果由 d 类型的 operator*(double)
     * 接口决定；当发生浮点数转整型值时，以四舍五入方式取整。
     *
     * @tparam T 参数的类型
     * @param d 要被缩放的值
     * @return T 对 d 进行缩放后的值
     */
    template <class T>
    T Scale(T d) const
    {
        return CoordCvt<T, double>(d);
    }

    /**
     * @brief 对坐标 p 进行变换，这将改变 p 的值。
     *
     * @param p 要变换的坐标
     * @return Point<CoordType> 变换后的坐标
     */
    template <class CoordType>
    Point<CoordType> &Transform(Point<CoordType> &p) const
    {
        if constexpr (std::is_integral_v<CoordType>) {
            // 在溢出时，取溢出方向的最大/最小值，这样使得在区域计算时，更符合预期
            if (CheckIntegerAddOverflow(p.X(), translation_.X())) {
                if (p.X() > 0) {
                    p.SetX(std::numeric_limits<CoordType>::max());
                } else {
                    p.SetX(std::numeric_limits<CoordType>::lowest());
                }
            } else {
                p.SetX(p.X() + translation_.X());
            }
            if (CheckIntegerAddOverflow(p.Y(), translation_.Y())) {
                if (p.Y() > 0) {
                    p.SetY(std::numeric_limits<CoordType>::max());
                } else {
                    p.SetY(std::numeric_limits<CoordType>::lowest());
                }
            } else {
                p.SetY(p.Y() + translation_.Y());
            }
            return p;
        } else {
            PointD pd(p);
            pd = pd + translation_;

            return CheckOverflow(pd, p);
        }
    }

    /**
     * @brief 对坐标 p 进行变换，不会改变 p 的值。
     *
     * @param p 要变换的坐标
     * @return Point<CoordType> 变换后的坐标
     */
    template <class CoordType>
    Point<CoordType> Transformed(const Point<CoordType> &p) const
    {
        Point<CoordType> other_p = p;
        return Transform(other_p);
    }

    /**
     * @brief 获取位移向量的值
     *
     * @return const Vector<int32_t>& 位移向量对象
     */
    const Vector<int32_t> &Translation() const { return translation_; }

    /**
     * @brief 获取位移向量的值
     *
     * @return Vector<int32_t>& 位移向量对象
     */
    Vector<int32_t> &Translation() { return translation_; }

    /**
     * @brief 获取旋转角度
     *
     * @return RotationType 旋转角度
     */
    RotationType Rotation() const { return kRotation0; }

    /**
     * @brief 获取缩放系数及是否镜像信息
     *
     * @return double 缩放与镜像信息, 正值表示无镜像，负值表示需要相对 X 轴进行镜像
     */
    double Magnification() const { return 1.0; }

    /**
     * @brief 重载比较操作符
     *
     * @param other 另一个要比较的对象
     * @return true 相等
     * @return false 不相等
     */
    bool operator==(const SimpleTransformation &other) const { return translation_ == other.translation_; }

    /**
     * @brief 重载比较操作符
     *
     * @param other 另一个要比较的对象
     * @return true 不相等
     * @return false 相等
     */
    bool operator!=(const SimpleTransformation &other) const { return !(*this == other); }

    /**
     * @brief 将 SimpleTransformation 转换为 strign，用于 DEBUG
     *
     * @return std::string SimpleTransformation 对象字符串形式值
     */
    std::string ToString() const { return "Vector:" + translation_.ToString(); }

    /**
     * @brief 计算 SimpleTransformation 的逆变换（比如某个点在Top cell中的坐标为coord_top, 在sub
     * cell中的坐标为coord_sub, 假如，coord_sub.Transformed(trans)就等于coord_top,
     * 那么，coord_top.Transformed(trans.Invert())就等于 coord_sub）
     *
     * @return SimpleTransformation 返回当前变换的逆变换
     */
    SimpleTransformation Inverted() const
    {
        SimpleTransformation inv;
        inv.translation_ = -translation_;
        return inv;
    }

private:
    Vector<int32_t> translation_;  // 平移
};

/**
 * @brief 坐标转换类，为 Point 提供位移、90° 整数倍的角度旋转、缩放及相对 X 轴镜像的功能
 *
 * @tparam CoordType 坐标类型，参考 Point
 */
class Transformation {
public:
    Transformation() = default;
    ~Transformation() = default;
    Transformation(const Transformation &) = default;
    Transformation &operator=(const Transformation &) = default;

    /**
     * @brief 带参构造函数
     *
     * @param translation 位移向量
     * @param rotation 旋转角度，逆时针方向
     * @param magnification 缩放系数，正值表示无镜像，负值表示需要相对 X 轴进行镜像
     */
    Transformation(const Vector<double> &translation, RotationType rotation, double magnification)
        : translation_(translation), magnification_(magnification), rotation_(rotation)
    {
    }

    /**
     * @brief 设置位移、旋转角度、缩放系数及是否镜像的参数
     *
     * @param translation 位移向量
     * @param rotation 旋转角度，逆时针方向为正，顺时针方向为负
     * @param magnification 缩放系数，正值表示无镜像，负值表示需要相对 X 轴进行镜像
     */
    void Set(const Vector<double> &translation, RotationType rotation, double magnification)
    {
        translation_ = translation;
        magnification_ = magnification;
        rotation_ = rotation;
    }

    /**
     * @brief 对坐标 p 进行变换，这将改变 p 的值。
     * 变换的步骤依次为：1.镜像；2.缩放；3.旋转；4.位移。此操作将改变 p 的值。
     * 若有溢出则按参数原值返回。转换时如果发生浮点型向整型转换，则以四舍五入方式取整。
     *
     * @param p 要变换的坐标
     * @return Point<CoordType> 变换后的坐标
     */
    template <class CoordType>
    Point<CoordType> &Transform(Point<CoordType> &p) const
    {
        PointD pd(p);
        if (DoubleLess(magnification_, 0.0)) {
            pd.SetY(-pd.Y());
        }
        pd = pd * fabs(magnification_);
        pd = RotatePoint(pd, rotation_) + translation_;

        return CheckOverflow(pd, p);
    }

    /**
     * @brief 对坐标 p 进行变换，不会改变 p 的值。
     * 变换的步骤依次为：1.镜像；2.缩放；3.旋转；4.位移，其中 1 和 2 是可以同时进行。
     * 若有溢出则按参数原值返回。转换时如果发生浮点型向整型转换，则以四舍五入方式取整。
     *
     * @param p 要变换的坐标
     * @return Point<CoordType> 变换后的坐标
     */
    template <class CoordType>
    Point<CoordType> Transformed(const Point<CoordType> &p) const
    {
        Point<CoordType> other_p = p;
        return Transform(other_p);
    }

    /**
     * @brief 对参数 d 进行缩放，缩放系数为 magnification_ 的绝对值；d
     * 的类型可以为非数值类型，当为非数值类型时，运算结果由 d 类型的 operator*(double)
     * 接口决定；当发生浮点数转整型值时，以四舍五入方式取整。
     *
     * @tparam T 参数的类型
     * @param d 要被缩放的值
     * @return T 对 d 进行缩放后的值
     */
    template <class T>
    T Scale(T d) const
    {
        return CoordCvt<T, double>(d * fabs(magnification_));
    }

    /**
     * @brief 获取位移向量的值
     *
     * @return const Vector<double>& 位移向量对象
     */
    const Vector<double> &Translation() const { return translation_; }

    /**
     * @brief 获取位移向量的值
     *
     * @return Vector<double>& 位移向量对象
     */
    Vector<double> &Translation() { return translation_; }

    /**
     * @brief 获取旋转角度
     *
     * @return RotationType 旋转角度
     */
    RotationType Rotation() const { return rotation_; }

    /**
     * @brief 获取缩放系数及是否镜像信息
     *
     * @return double 缩放与镜像信息, 正值表示无镜像，负值表示需要相对 X 轴进行镜像
     */
    double Magnification() const { return magnification_; }

    /**
     * @brief 重载比较操作符
     *
     * @param other 另一个要比较的对象
     * @return true 相等
     * @return false 不相等
     */
    bool operator==(const Transformation &other) const
    {
        return (translation_ == other.translation_) && (rotation_ == other.rotation_) &&
               DoubleEqual(magnification_, other.magnification_);
    }

    /**
     * @brief 重载比较操作符
     *
     * @param other 另一个要比较的对象
     * @return true 不相等
     * @return false 相等
     */
    bool operator!=(const Transformation &other) const { return !(*this == other); }

    /**
     * @brief 将 Transformation 转换为 strign，用于 DEBUG
     *
     * @return std::string Transformation 对象字符串形式值
     */
    std::string ToString() const
    {
        return "Vector:" + translation_.ToString() + "\nRotation:" + std::to_string(rotation_ * kDegrees90) +
               "\nMagnification:" + std::to_string(Magnification());
    }

    /**
     * @brief 计算 Transformation 的逆变换（比如某个点在Top cell中的坐标为coord_top, 在sub cell中的坐标为coord_sub,
     * 假如，coord_sub.Transformed(trans)就等于coord_top, 那么，coord_top.Transformed(trans.Invert())就等于
     * coord_sub）
     *
     * @return Transformation 返回当前变换的逆变换
     */
    Transformation Inverted() const
    {
        Transformation inv;
        inv.magnification_ = 1.0 / magnification_;
        if (DoubleGreater(magnification_, 0.0)) {
            inv.rotation_ = static_cast<RotationType>((kRotation360 - rotation_) % kRotation360);
        } else {
            inv.rotation_ = rotation_;
        }
        inv.translation_ = inv.Transformed(-translation_);
        return inv;
    }

private:
    Vector<double> translation_{0.0, 0.0};  // 平移
    double magnification_{1.0};             // 缩放
    RotationType rotation_{kRotation0};     // 旋转
};

template <class CoordType>
inline Point<CoordType> &CheckOverflow(const PointD &pd, Point<CoordType> &p)
{
    if constexpr (std::is_integral<CoordType>::value) {
        constexpr double half = 0.5;  // 0.5 用于解决INT_MAX场景的精度问题
        // 在溢出时，取溢出方向的最大/最小值，这样使得在区域计算时，更符合预期
        if (pd.X() > std::numeric_limits<CoordType>::max() + half) {
            p.SetX(std::numeric_limits<CoordType>::max());
        } else if (pd.X() < std::numeric_limits<CoordType>::lowest() - half) {
            p.SetX(std::numeric_limits<CoordType>::lowest());
        } else {
            p.SetX(CoordCvt<CoordType>(pd.X()));
        }
        if (pd.Y() > std::numeric_limits<CoordType>::max() + half) {
            p.SetY(std::numeric_limits<CoordType>::max());
        } else if (pd.Y() < std::numeric_limits<CoordType>::lowest() - half) {
            p.SetY(std::numeric_limits<CoordType>::lowest());
        } else {
            p.SetY(CoordCvt<CoordType>(pd.Y()));
        }
        return p;
    }
    p.SetX(CoordCvt<CoordType>(pd.X()));
    p.SetY(CoordCvt<CoordType>(pd.Y()));
    return p;
}
using TransformationVar = std::variant<SimpleTransformation, Transformation>;
}  // namespace medb2
#endif  // MEDB_TRANSFORMATION_H
