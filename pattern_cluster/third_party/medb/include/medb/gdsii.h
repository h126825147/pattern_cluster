/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_GDSII_H
#define MEDB_GDSII_H

#include "layout.h"
#include "parser_utils.h"

namespace medb2 {
namespace gdsii {

/**
 * @brief 加载 GDSII 文件时使用的配置参数
 */
class GdsiiReadOption {
public:
    void SetLayerTypes(const std::unordered_map<int16_t, std::set<int16_t>> &layer_types)
    {
        layer_types_ = layer_types;
    }
    void AddLayerType(int16_t layer_id, int16_t type_id) { layer_types_[layer_id].insert(type_id); }
    const std::unordered_map<int16_t, std::set<int16_t>> &LayerTypes() const { return layer_types_; }

    void SetThreadNum(uint32_t thread_num) { thread_num_ = thread_num; }
    uint32_t ThreadNum() const { return thread_num_; }

    void SetIsSkipText(bool is_skip_text) { is_skip_text_ = is_skip_text; }
    bool IsSkipText() const { return is_skip_text_; }

    void SetIsPathToPolygon(bool is_path_to_polygon) { is_path_to_polygon_ = is_path_to_polygon; }
    bool IsPathToPolygon() const { return is_path_to_polygon_; }

    void SetIsReadZeroAreaShape(bool is_read_zero_area_shape) { is_read_zero_area_shape_ = is_read_zero_area_shape; }
    bool IsReadZeroAreaShape() const { return is_read_zero_area_shape_; }

    void SetReadProgressCallBack(ReadProgressCallback func) { read_progress_call_back_ = func; }
    ReadProgressCallback GetReadProgressCallBack() const { return read_progress_call_back_; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    /**
     * @brief 只加载用户指定的 layer。若为空则加载所有 layer
     */
    std::unordered_map<int16_t, std::set<int16_t>> layer_types_;

    /**
     * @brief 指定加载时使用的线程数量，仅对大于 1M 的文件生效
     */
    uint32_t thread_num_{1};

    /**
     * @brief 是否过滤掉 text
     */
    bool is_skip_text_{false};

    /**
     * @brief 加载时是否将 path 转为 polygon
     */
    bool is_path_to_polygon_{false};

    /**
     * @brief 加载时是否读入无面积的图形
     */
    bool is_read_zero_area_shape_{false};

    /**
     * @brief 加载时，可选择设置的读取进度信息的回调函数
     */
    ReadProgressCallback read_progress_call_back_{nullptr};
};

/**
 * @brief 导出 GDSII 文件时使用的配置参数
 */
class GdsiiWriteOption {
public:
    void SetWriteDbu(double dbu) { write_dbu_ = dbu; }
    double WriteDbu() const { return write_dbu_; }

    void SetThreadNum(uint32_t thread_num) { thread_num_ = thread_num; }
    uint32_t ThreadNum() const { return thread_num_; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    /**
     * @brief 用户指定的 dbu，每单位表示的微米数，有效值范围 [1e-9, 1000.0]，例如 0.001
     */
    double write_dbu_{0.0};

    /**
     * @brief 导出时,指定的使用的线程数量
     */
    uint32_t thread_num_{1};
};

/**
 * @brief 加载指定的 GDSII 文件
 *
 * @param path 待加载的 GDSII 文件
 * @param option 加载 GDSII 文件时使用的配置参数
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *Read(const std::string &path, const GdsiiReadOption &option = {});

/**
 * @brief 将 Layout 对象中的数据导出为 GDSII 文件
 *
 * @param layout 被导出的 layout 指针对象
 * @param path 导出的目标 GDSII 文件
 * @param option 导出的目标 GDSII 文件的配置参数
 * @return 成功则返回 true，失败则返回 false
 */
bool MEDB_API Write(Layout *layout, const std::string &path, const GdsiiWriteOption &option = {});

/**
 * @brief 判断指定的文件是否为 GDSII 文件
 *
 * @return 是 GDSII 文件则返回 true，否则返回 false
 */
bool MEDB_API IsGdsiiFile(const std::string &file_name);

MEDB_RESULT MEDB_API DdePreprocess(const DdeOption &dde_opt);
}  // namespace gdsii
}  // namespace medb2

#endif  // MEDB_GDSII_H
