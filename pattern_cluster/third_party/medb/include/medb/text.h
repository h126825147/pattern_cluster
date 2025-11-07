/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_TEXT_H
#define MEDB_TEXT_H

#include <cstring>
#include "medb/allocator.h"
#include "medb/base_utils.h"
#include "medb/enums.h"
#include "medb/geometry_utils.h"
#include "medb/transformation.h"

namespace medb2 {

/**
 * @brief 描述一个2D空间的文本
 *
 */
class Text {
public:
    // 默认构造函数
    Text()
        : str_(""),
          trans_(),
          font_(FontType::kInvalidFont),
          horizon_align_(HorizonAlignType::kInvalidHorizonAlign),
          vertical_align_(VerticalAlignType::kInvalidVerticalAlign)
    {
    }

    // 析构函数
    ~Text() {}

    /**
     * @brief 带参构造函数
     *
     * @param str 字符串信息
     * @param t Trans信息
     * @param f 字体信息
     * @param h 水平方向位置信息
     * @param v 垂直方向位置信息
     */
    Text(const std::string &str, const Transformation &t, FontType f, HorizonAlignType h, VerticalAlignType v)
        : str_(str), trans_(t), font_(f), horizon_align_(h), vertical_align_(v)
    {
    }

    Text(const Text &) = default;
    Text(Text &&) = default;

    Text &operator=(const Text &) = default;
    Text &operator=(Text &&) = default;

    /**
     * @brief 设置或变更Text的字符串内容
     *
     * @param str 准备设置的字符串
     */
    void SetString(const std::string &str) { str_ = str; }

    /**
     * @brief 设置或改变Transformation信息
     *
     * @param t 准备设置或改变的Transformation对象
     */
    void SetTransformation(const Transformation &t) { trans_ = t; }

    /**
     * @brief 设置或改变字体信息
     *
     * @param f 准备设置或改变的字体信息
     */
    void SetFont(FontType f) { font_ = f; }

    /**
     * @brief 设置或改变对齐值
     *
     * @param h 准备设置或改变的水平方向位置信息
     * @param v 准备设置或改变的垂直方向位置信息
     */
    void SetAlign(HorizonAlignType h, VerticalAlignType v)
    {
        horizon_align_ = h;
        vertical_align_ = v;
    }

    /**
     * @brief Text==运算符重载 判断两个Text是否相等
     *
     * @param t 与之判断不等的Text对象引用
     * @return 相等返回true, 不相等返回false
     */
    bool operator==(const Text &t) const
    {
        return (str_ == t.str_ && trans_ == t.trans_ && font_ == t.font_ && horizon_align_ == t.horizon_align_ &&
                vertical_align_ == t.vertical_align_);
    }

    /**
     * @brief Text!=运算符重载 判断两个Text是否不等
     *
     * @param t 与之判断不等的Text对象引用
     * @return 不等返回true, 相等返回false
     */
    bool operator!=(const Text &t) const { return !(*this == t); }

    /**
     * @brief Text<运算符重载 比较一个text是否另一个text
     * 比较方法：首先比较两个text的位置信息,其次比较字符串信息,之后比较其余信息
     *
     * @param t 与之判断小于的Text对象引用
     * @return 小于返回true, 大于返回false
     */
    bool operator<(const Text &t) const
    {
        if (trans_.Translation() != t.trans_.Translation()) {
            return trans_.Translation() < t.trans_.Translation();
        }

        if (str_ != t.str_) {
            int c = strcmp(str_.c_str(), t.str_.c_str());
            if (c != 0) {
                return c < 0;
            }
        }

        if (font_ != t.font_) {
            return font_ < t.font_;
        }
        if (horizon_align_ != t.horizon_align_) {
            return horizon_align_ < t.horizon_align_;
        }
        if (vertical_align_ != t.vertical_align_) {
            return vertical_align_ < t.vertical_align_;
        }
        return false;
    }

    /**
     * @brief 获取Text字符串信息
     *
     * @return const std::string& 获取到的Text字符串信息
     */
    const std::string &String() const { return str_; }

    /**
     * @brief 获取Text的Transformation信息
     *
     * @return const Transformation& 获取到的Text的Transformation对象引用
     */
    const Transformation &Trans() const { return trans_; }

    /**
     * @brief 获取Text的字体信息
     *
     * @return FontType 获取到的Text的字体信息
     */
    FontType Font() const { return font_; }

    /**
     * @brief 获取Text字体的水平位置信息
     *
     * @return HorizonAlignType 获取到的Text字体水平位置信息
     */
    HorizonAlignType HorizonAlign() const { return horizon_align_; }

    /**
     * @brief 获取Text字体的垂直方向位置信息
     *
     * @return VerticalAlignType 获取到的Text字体垂直方向位置信息
     */
    VerticalAlignType VerticalAlign() const { return vertical_align_; }

    /**
     * @brief Text改变自身做变换
     *
     * @param t 包含变换信息的Transformation对象引用
     * @return Text& 返回自身引用
     */
    template <class TransType>
    Text &Transform(const TransType &t)
    {
        Vector<double> translation = trans_.Translation();
        PointD position(translation.X(), translation.Y());
        PointD new_position = t.Transform(position);
        trans_.Set(new_position, trans_.Rotation(), trans_.Magnification());
        return *this;
    }

    /**
     * @brief Text不改变自身做变换
     *
     * @param t 包含变换信息的Transformation对象引用
     * @return Text 变换后Text对象引用
     */
    template <class TransType>
    Text Transformed(const TransType &t) const
    {
        Vector<double> translation = trans_.Translation();
        PointD position(translation.X(), translation.Y());
        PointD new_position = t.Transform(position);
        Transformation trans(new_position, trans_.Rotation(), trans_.Magnification());
        return Text(str_, trans, font_, horizon_align_, vertical_align_);
    }

    /**
     * @brief Text信息转换成字符串
     *
     * @return std::string Text内部数据的字符串描述
     */
    std::string ToString() const
    {
        std::string str;
        str.append("Text:")
            .append(str_)
            .append("\n")
            .append(trans_.ToString())
            .append("\n")
            .append("Font:")
            .append(std::to_string(EToI(font_)))
            .append("\n")
            .append("HorizonAlign:")
            .append(std::to_string(EToI(horizon_align_)))
            .append("\n")
            .append("VerticalAlign:")
            .append(std::to_string(EToI(vertical_align_)));
        return str;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    // 成员变量
    std::string str_;
    Transformation trans_;
    FontType font_;
    HorizonAlignType horizon_align_;
    VerticalAlignType vertical_align_;
};

}  // namespace medb2

#endif  // MEDB_TEXT_H
