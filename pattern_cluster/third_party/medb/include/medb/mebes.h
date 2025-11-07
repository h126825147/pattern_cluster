/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef MEDB_MEBES_H
#define MEDB_MEBES_H

#include "layout.h"

namespace medb2 {
namespace mebes {

/**
 * @brief 加载 Mebes 文件时使用的配置参数
 */
class MebesReadOption {
public:
    void SetCellStripeNum(uint32_t num) { cell_stripe_num_ = num; }
    uint32_t CellStripeNum() const { return cell_stripe_num_; }

    void SetCellShapesNum(uint32_t num) { cell_shapes_num_ = num; }
    uint32_t CellShapesNum() const { return cell_shapes_num_; }

    void SetShapesLayer(const Layer &layer) { shapes_layer_ = layer; }
    const Layer &ShapesLayer() const { return shapes_layer_; }

    void SetIsNeedChipBox(bool need) { is_need_chip_box_ = need; }
    bool IsNeedChipBox() const { return is_need_chip_box_; }

    void SetChipBoxLayer(const Layer &layer) { chip_box_layer_ = layer; }
    const Layer &ChipBoxLayer() const { return chip_box_layer_; }

    void SetTopCellName(const std::string &name)
    {
        if (!name.empty()) {
            top_cell_name_ = name;
        }
    }
    const std::string &TopCellName() const { return top_cell_name_; }

    void SetIsScaleInstance(bool scale) { is_scale_instance_ = scale; }
    bool IsScaleInstance() const { return is_scale_instance_; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    uint32_t cell_stripe_num_{64};  // 指定 Cell 中最大条纹数
    uint32_t cell_shapes_num_{
        std::numeric_limits<uint32_t>::max()};  // 指定 Cell 中 Shapes 阈值，默认为 max，表示不按此值切分 Cell
    Layer shapes_layer_{1, 0};                  // 指定存放 shapes 的 Layer
    bool is_need_chip_box_{true};               // 是否存储 chip box 信息
    Layer chip_box_layer_{0, 0};                // 指定存放 chip bbox 的 Layer，仅在需要存储 chip box 时生效
    std::string top_cell_name_{"MEDB-MEBES"};   // 指定 top cell 的名称
    bool is_scale_instance_{true};              // 生成的 instance 是否需要做 1/16 倍的 transform 变换
};

/**
 * @brief 加载指定的 Mebes 文件
 *
 * @param path 待加载的 Mebes 文件
 * @param option 加载 Mebes 文件时使用的配置参数
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *Read(const std::string &path, const MebesReadOption &option = {});

/**
 * @brief 导出 Mebes 文件时使用的配置参数
 */
class MebesWriteOption {
public:
    void SetStripeAddressUnit(double unit) { stripe_address_unit_ = unit; }
    double StripeAddressUnit() const { return stripe_address_unit_; }

    void SetStripeHeight(uint16_t height) { stripe_height_ = height; }
    uint16_t StripeHeight() const { return stripe_height_; }

    void SetChipWidth(uint32_t width) { chip_width_ = width; }
    uint32_t ChipWidth() const { return chip_width_; }

    void SetChipHeight(uint32_t height) { chip_height_ = height; }
    uint32_t ChipHeight() const { return chip_height_; }

    void SetPatternName(const std::string &name) { pattern_name_ = name; }
    const std::string &PatternName() const { return pattern_name_; }

    void SetMaskShopInfo(const std::string &info) { mask_shop_info_ = info; }
    const std::string &MaskShopInfo() const { return mask_shop_info_; }

    void SetData(const std::vector<std::string> &data) { data_ = data; }
    void AddData(const std::string &data) { data_.emplace_back(data); }
    const std::vector<std::string> &Data() const { return data_; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    double stripe_address_unit_{0.0};  // 必须大于 0
    uint16_t stripe_height_{0};        // stripe 的高，必须是 2 的幂，且取值范围[256, 1024]
    uint32_t chip_width_{0};           // 芯片高度
    uint32_t chip_height_{0};          // 芯片宽度
    std::string pattern_name_;    // 数字或字母的组合，以大写形式写出字母。固定 11 个字符，超处部分被截断，不足时补 `X`
    std::string mask_shop_info_;  // 固定 40 个字符，超出部分被截断，不足时补空格
    std::vector<std::string> data_;  // 其它 property 信息，可以为空
};

/**
 * @brief 将指定 Shapes 集合导出至 option 中指定的文件
 *
 * @param shapes 待导出的 Shapes 集合，形如 shapes[segment][stripe]
 * @param path 导出文件的路径
 * @param option 导出的配置参数
 * @return 成功则返回 true，否则返回 false
 */
bool MEDB_API Write(const std::vector<std::vector<const Shapes *>> &shapes, const std::string &path,
                    const MebesWriteOption &option);
}  // namespace mebes
}  // namespace medb2

#endif  // MEDB_MEBES_H
