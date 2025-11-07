/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef PROPERTY_INDEX_H
#define PROPERTY_INDEX_H

#include <vector>
#include "medb/element.h"
#include "medb/interval.h"

namespace medb2 {

class PropertyIndex {
public:
    PropertyIndex() = default;
    ~PropertyIndex() = default;
    /**
     * @brief 构造属性存储表
     *
     * @param elements 待存储属性的元素列表
     */
    explicit PropertyIndex(const MeVector<Element> &elements)
    {
        // support: polygon, box, path
        InitProperties(elements);
    }

    /**
     * @brief 根据属性阈值上下限，以及目标图形类型，查询目标图形集合
     *
     * @param property_type       属性类型
     * @param interval            属性上下限值
     * @return 符合阈值范围以及图形类型的图形集合
     */
    template <class CoordType>
    MeVector<Element> Query(ElementPropertyType property_type, const Interval<CoordType> &interval)
    {
        MeVector<Element> ret;
        auto lower = interval.Lower();
        auto upper = interval.Upper();
        if (property_type == ElementPropertyType::kArea) {
            if (DoubleGreater(lower, upper)) {
                return ret;
            }
            auto lower_it = elements_area_map_.lower_bound(lower);
            if (lower_it == elements_area_map_.end()) {
                return ret;
            }

            auto upper_it = elements_area_map_.upper_bound(upper);

            if (!interval.LowerClose()) {
                while (DoubleEqual(lower_it->first, lower) && lower_it != elements_area_map_.end()) {
                    lower_it++;
                }
            }
            if (!interval.UpperClose()) {
                while (upper_it == elements_area_map_.end() || DoubleGreaterEqual(upper_it->first, upper)) {
                    upper_it--;
                }
                if (upper_it != elements_area_map_.end() && DoubleLess(upper_it->first, upper)) {
                    upper_it++;
                }
            }

            for (auto &it = lower_it; it != upper_it; ++it) {
                ret.insert(ret.end(), it->second.begin(), it->second.end());
            }
        }
        return ret;
    }

private:
    void InitProperties(const MeVector<Element> &elements)
    {
        elements_area_map_.clear();
        for (const auto &element : elements) {
            elements_area_map_[element.Area()].emplace_back(element);
        }
    }

    MeMap<double, MeVector<Element>> elements_area_map_;
};

}  // namespace medb2
#endif  // PROPERTY_INDEX_H
