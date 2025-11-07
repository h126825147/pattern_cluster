/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_OASIS_H
#define MEDB_OASIS_H

#include "medb/layout.h"
#include "parser_utils.h"

namespace medb2 {
namespace oasis {
/**
 * @brief 加载 OASIS 文件时使用的配置参数
 */
class OasisReadOption {
public:
    void SetLayerTypes(const std::unordered_map<uint32_t, std::set<uint32_t>> &layer_types)
    {
        layer_types_ = layer_types;
    }
    void AddLayerType(uint32_t layer_id, uint32_t type_id) { layer_types_[layer_id].insert(type_id); }
    const std::unordered_map<uint32_t, std::set<uint32_t>> &LayerTypes() const { return layer_types_; }

    void SetCellOffsets(const std::vector<uint64_t> &offsets) { cell_offsets_ = offsets; }
    void AddCellOffset(uint64_t offset) { cell_offsets_.emplace_back(offset); }
    const std::vector<uint64_t> &CellOffsets() const { return cell_offsets_; }

    void SetThreadNum(uint32_t thread_num) { thread_num_ = thread_num; }
    uint32_t ThreadNum() const { return thread_num_; }

    void SetIsCheckRepetitionInTable(bool is_check) { is_check_repetition_in_table_ = is_check; }
    bool IsCheckRepetitionInTable() const { return is_check_repetition_in_table_; }

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
    std::unordered_map<uint32_t, std::set<uint32_t>> layer_types_;

    /**
     * @brief 只加载用户给定的 offset 上的 Cell。若为空则加载所有 Cell
     */
    std::vector<uint64_t> cell_offsets_;

    /**
     * @brief 指定加载时使用的线程数量，仅对 strict mode 文件生效
     */
    uint32_t thread_num_{0};

    /**
     * @brief 指定加载 OASIS 文件时，是否校验存在重复的 ‘reference number’，‘TextString’，‘PropName’，‘PropString’
     */
    bool is_check_repetition_in_table_{false};

    /**
     * @brief 是否过滤掉 text
     */
    bool is_skip_text_{false};

    /**
     * @brief 加载时是否将 path 转为 polygon
     */
    bool is_path_to_polygon_{false};

    /**
     * @brief 是否最终读成一个框架layout(保留cell的hier层级，但每个cell仅有一个box图元，代表其bounding box)
     */
    bool is_read_as_frame_layout_{false};

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
 * @brief 导出 OASIS 时的配置参数
 */
class OasisWriteOption {
public:
    void SetWriteDbu(double dbu) { write_dbu_ = dbu; }
    double WriteDbu() const { return write_dbu_; }

    void SetCompressLevel(uint32_t level) { compress_level_ = level; }
    uint32_t CompressLevel() const { return compress_level_; }

    void SetIsWithoutReference(bool is_without_reference) { is_without_reference_ = is_without_reference; }
    bool IsWithoutReference() const { return is_without_reference_; }

    void SetIsWriteCblock(bool is_write_cblock) { is_write_cblock_ = is_write_cblock; }
    bool IsWriteCblock() const { return is_write_cblock_; }

    void SetLayers(const std::unordered_set<Layer> &layers) { layers_ = layers; }
    const std::unordered_set<Layer> &Layers() const { return layers_; }

    void SetRegions(const std::vector<BoxI> &regions) { regions_ = regions; };
    const std::vector<BoxI> &Regions() const { return regions_; }

    void SetClip(bool clip) { clip_ = clip; }
    bool Clip() const { return clip_; }

    void SetThreadNum(uint32_t thread_num) { thread_num_ = thread_num; }
    uint32_t ThreadNum() const { return thread_num_; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    // 用户指定的 dbu，每单位表示的微米数，有效值范围 [1e-9, 1000.0]，例如 0.001
    double write_dbu_{0.0};

    // 指定压缩等级，有效值范围 [0, 5], 0 表示不压缩
    uint32_t compress_level_{0};

    // 是否使用 ‘reference number’
    bool is_without_reference_{false};

    // 导出时是否开启 cblock 功能
    bool is_write_cblock_{true};

    // 指定要导出的 layers, 默认导出全部 layers，若指定了 layers，则仅会导出当前 TOP cell 及其后代，并且不会导出空 Cell
    std::unordered_set<Layer> layers_;

    // 指定导出的区域，默认不按区域导出，若指定了区域，则仅会导出当前 TOP cell 及其后代，并且不会导出空 Cell。不能存在
    // Empty 的区域，否则导出会失败。若是多线程导出，则需外部事先更新 bbox 的缓存。
    std::vector<BoxI> regions_;

    // 在指定导出区域时，标识是否按区域对图形裁剪(无 merge 效果)。仅在 regions_ 非空时有效
    bool clip_{false};

    // 导出时,指定的使用的线程数量
    uint32_t thread_num_{1};
};

/**
 * @brief OASIS 多文件合并导出的启动参数
 */
class StartMergeOption {
public:
    void SetWriteFileName(const std::string &file_name) { write_file_name_ = file_name; }
    const std::string &WriteFileName() const { return write_file_name_; }

    void SetTopCellName(const std::string &cell_name) { top_cell_name_ = cell_name; }
    const std::string &TopCellName() const { return top_cell_name_; }

    void AddProperties(const std::string &name, const std::vector<Properties::PropertyValueType> &values)
    {
        properties_map_.insert_or_assign(name, values);
    }
    const std::unordered_map<std::string, std::vector<Properties::PropertyValueType>> &PropertiesMap()
    {
        return properties_map_;
    }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    // 合并导出的文件路径
    std::string write_file_name_;
    // 合并导出时的 top cell 名称
    std::string top_cell_name_;
    // 需要写入合并导出文件中的 properties
    std::unordered_map<std::string, std::vector<Properties::PropertyValueType>> properties_map_;
};

/**
 * @brief OASIS 多文件合并导出的终止参数
 */
class EndMergeOption {
public:
    void AddLayerName(const Layer &layer, const std::string &name) { layer_names_.emplace_back(layer, name); }
    const std::vector<std::pair<Layer, std::string>> &LayerNames() const { return layer_names_; }

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    // Layer name 集合，导出时不会检查是否重复
    std::vector<std::pair<Layer, std::string>> layer_names_;
};

/**
 * @brief 判断给定的文件是否为 OASIS 格式数据
 *
 * @param file_name 待检测的文件
 * @return 若个给定的文件是 OASIS 格式数据，则返回 true，否则返回 false
 */
bool MEDB_API IsOasisFile(const std::string &file_name);

/**
 * @brief 加载指定的 OASIS 文件
 *
 * @param path 待加载的 OASIS 文件
 * @param option 加载 OASIS 文件时使用的配置参数
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *Read(const std::string &path, const OasisReadOption &option = {});

/**
 * @brief 从内存中加载  oasis 数据
 *
 * @param buffer 待加载的内存地址
 * @param buffer_size 内存数据的大小
 * @param option 加载 OASIS 文件时使用的配置参数
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *Read(const char *buffer, uint64_t buffer_size, const OasisReadOption &option = {});

/**
 * @brief 获取指定文件中所有 cell 的数据偏移位置
 *
 * @param path OASIS 文件路径
 * @param cell_name_cell_offset_array 出参，存储了各 cell 的偏移位置
 * @return 成功则返回 true，否则返回 false
 */
bool MEDB_API GetCellNameCellOffset(const std::string &path,
                                    std::vector<std::pair<std::string, uint64_t>> &cell_name_cell_offset_array);

/**
 * @brief 获取 OASIS 中的 UNIT、Layout 的 properties 以及 Layer 名称信息(oasis 文件的 layer-table 中的信息)
 *
 * @param file_path OASIS 文件路径
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *GetMetaData(const std::string &file_path);

/**
 * @brief 将 Layout 对象数据导出为 OASIS 文件
 * 注意，对同一个 layout 对象进行并行导出时，需要先更新各 Cell 的 Layer 与 BoundingBox 的缓存
 *
 * @param layout 待导出数据的 Layout 对象
 * @param path 导出的 OASIS 文件路径
 * @param option 导出的配置参数
 * @return 成功则返回 true，否则返回 false
 */
bool MEDB_API Write(Layout *layout, const std::string &path, const OasisWriteOption &option = {});

/**
 * @brief 将 Layout 导出为 OASIS 数据流
 * 注意，对同一个 layout 对象进行并行导出时，需要先更新各 Cell 的 Layer 与 BoundingBox 的缓存
 *
 * @param layout 待导出数据的 Layout 对象
 * @param str_stream 出参，用以存储 OASIS 数据流
 * @param option 导出的配置参数
 * @return 成功则返回 true，否则返回 false
 */
bool MEDB_API Write(Layout *layout, std::stringstream &str_stream, const OasisWriteOption &option = {});

class OasisImpl;
class MEDB_API OasisMerger {
public:
    OasisMerger();
    ~OasisMerger();

    /**
     * @brief 开启合并导出的功能，合并导出功能的一般调用流程为 StartMergeFiles->AddMergeFile->EndMergeFiles
     *
     * @param option 合并导出的配置参数
     * @return 成功则返回 true，否则返回 false
     */
    bool StartMergeFiles(const StartMergeOption &option);

    /**
     * @brief 添加一个待合并的 OASIS 文件，此文件格式必须是 strict 模式(在 Write 时指定
     * OasisWriteOption::without_reference_==true)
     *
     * @param file_name 待合并的 OASIS 文件
     * @return 成功则返回 true，否则返回 false
     */
    bool AddMergeFile(const std::string &file_name);
    /**
     * @brief 添加一个待合并的 OASIS 文件，此文件格式必须是 strict 模式(在 Write 时指定
     * OasisWriteOption::without_reference_==true)
     *
     * @param file_name 待合并的 OASIS 文件
     * @param excluded_cells 合并时被剔除的 Cell 集合
     * @return 成功则返回 true，否则返回 false
     */
    bool AddMergeFile(const std::string &file_name, const std::unordered_set<std::string> &excluded_cells);
    /**
     * @brief 添加一个待合并的 OASIS 格式的数据流
     *
     * @param buffer 待合并的 OASIS 内存数据流(实际就是将 OASIS 文件内容读到内存中了)
     * @param buffer_size buffer 中的数据长度
     * @return 成功则返回 true，否则返回 false
     */
    bool AddMergeFile(const char *buffer, const uint64_t buffer_size);
    /**
     * @brief 添加一个仅包含single instance的cell，single
     * instance引用的cell，必须是已经通过AddMergeFile或AddParentCell添加了的
     *
     * @param cell_name 待添加的cell的name
     * @param inst_info 待添加的cell所包含的所有single instance的信息。inst_info[i].first：single
     * instance引用的cell的name。inst_info[i].second:single instance的transform信息。
     * @return 成功则返回 true，否则返回 false
     */
    bool AddParentCell(const std::string &cell_name,
                       const std::vector<std::pair<std::string, TransformationVar>> &inst_info);

    /**
     * @brief 合并导出的结束动作，结束后不能再添加文件
     *
     * @param option 结束合并导出的配置参数
     * @return 成功则返回 true，否则返回 false
     */
    bool EndMergeFiles(const EndMergeOption &option = {});

    USE_MEDB_OPERATOR_NEW_DELETE

private:
    std::unique_ptr<OasisImpl> implementer_;
};

MEDB_RESULT MEDB_API DdePreprocess(const DdeOption &dde_opt);
}  // namespace oasis
}  // namespace medb2
#endif  // MEDB_OASIS_H
