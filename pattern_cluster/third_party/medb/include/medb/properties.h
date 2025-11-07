/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_PROPERTIES_H
#define MEDB_PROPERTIES_H

#include <algorithm>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "medb/base_utils.h"

namespace medb2 {

class MEDB_API Properties {
public:
    using PropertyValueType = int64_t;
    Properties() = default;
    ~Properties() = default;

    explicit Properties(const std::unordered_map<std::string, std::vector<PropertyValueType>> &properties)
        : properties_(properties)
    {
    }

    Properties(const Properties &other) = default;
    Properties(Properties &&other) noexcept = default;

    Properties &operator=(const Properties &other) = default;
    Properties &operator=(Properties &&other) noexcept = default;

    bool operator==(const Properties &other) const { return properties_ == other.properties_; }

    /**
     * @brief 修改属性的属性值, 若 name 不存在则新增
     *
     * @param prop_name 属性的名字
     * @param prop_values 修改后的属性值
     */
    void SetProperty(const std::string &prop_name, const std::vector<PropertyValueType> &prop_values)
    {
        properties_[prop_name] = prop_values;
    }

    /**
     * @brief 获取名字对应的属性值
     *
     * @param prop_name 所需获取的属性值的属性名字
     * @return 当前属性名字下所有的属性值
     */
    const std::vector<PropertyValueType> GetProperty(const std::string &prop_name) const
    {
        auto it = properties_.find(prop_name);
        if (it != properties_.end()) {
            return it->second;
        } else {
            return {};
        }
    }

    /**
     * @brief 获取所有属性
     *
     * @return 当前对象中的所有属性
     */
    const std::unordered_map<std::string, std::vector<PropertyValueType>> &GetProperties() const { return properties_; }

    /**
     * @brief 清空当前对象
     */
    void Clear() { properties_.clear(); }

    // 打印所有属性
    std::string ToString() const
    {
        std::string text = "{\n";
        for (const auto &[key, values] : properties_) {
            text += " " + key + " : [";
            for (const auto &value : values) {
                text += std::to_string(value);
                text += ", ";
            }
            if (!values.empty()) {
                text.pop_back();
                text.pop_back();
            }
            text += "]\n";
        }
        text += "}\n";
        return text;
    }

private:
    std::unordered_map<std::string, std::vector<PropertyValueType>> properties_;
};

}  // namespace medb2

#endif  // MEDB_PROPERTIES_H
