/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef MEDB_HGS_H
#define MEDB_HGS_H

#include "layout.h"

namespace medb2 {
namespace hgs {
enum class HgsCBlockType : uint8_t { kNoCompress = 0, kZstd = 1, kLz4 = 2 };

/**
 * @brief 加载 Hgs 文件时使用的配置参数
 */
class HgsReadOption {
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

    void SetIsSkipText(bool is_skip_text) { is_skip_text_ = is_skip_text; }
    bool IsSkipText() const { return is_skip_text_; }

    void SetIsPathToPolygon(bool is_path_to_polygon) { is_path_to_polygon_ = is_path_to_polygon; }
    bool IsPathToPolygon() const { return is_path_to_polygon_; }

    void SetIsReadZeroAreaShape(bool is_read_zero_area_shape) { is_read_zero_area_shape_ = is_read_zero_area_shape; }
    bool IsReadZeroAreaShape() const { return is_read_zero_area_shape_; }

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
};

/**
 * @brief 检测给定的文件是否为 hgs 文件。通过文件首尾的 magic-bytes 校验
 *
 * @param file_name hgs 文件
 * @return 是 hgs 文件则返回 true, 否则返回 false
 */
bool MEDB_API IsHgsFile(const std::string &file_name);

/**
 * @brief 加载指定的 hgs 文件
 *
 * @param path 待加载的 hgs 文件
 * @param option 加载 hgs 文件时使用的配置参数
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *Read(const std::string &path, const HgsReadOption &option = {});

/**
 * @brief 从内存中加载  hgs 数据
 *
 * @param buffer 待加载的内存地址
 * @param buffer_size 内存数据的大小
 * @param option 加载 hgs 文件时使用的配置参数
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *Read(const char *buffer, uint64_t buffer_size, const HgsReadOption &option = {});

/**
 * @brief 获取指定文件中所有 cell 的数据偏移位置
 *
 * @param path HGS 文件路径
 * @param cell_name_cell_offset_array 出参，存储了各 cell 的偏移位置
 * @return 成功则返回 true，否则返回 false
 */
bool MEDB_API GetCellNameCellOffset(const std::string &path,
                                    std::vector<std::pair<std::string, uint64_t>> &cell_name_cell_offset_array);

/**
 * @brief 获取 HGS 中的 UNIT、Layout 的 properties 以及 Layer 信息(hgs 文件的 layer-table 中的信息)
 *
 * @param file_path HGS 文件路径
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *GetMetaData(const std::string &file_path);

/**
 * @brief 导出 Hgs 时的配置参数
 */
class HgsWriteOption {
public:
    void SetCompressLevel(uint32_t level) { compress_level_ = level; }
    uint32_t CompressLevel() const { return compress_level_; }

    void SetCBlockType(HgsCBlockType type) { c_block_type_ = type; }
    HgsCBlockType CBlockType() const { return c_block_type_; }

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
    // 指定压缩等级，有效值范围 [0, 5], 0 表示不压缩
    uint32_t compress_level_{0};

    // 压缩数据的方式
    HgsCBlockType c_block_type_{HgsCBlockType::kNoCompress};

    // 指定要导出的 layers, 默认导出全部 layers，若指定了 layers，则仅会导出当前 TOP cell 及其后代，并且不会导出空 Cell
    std::unordered_set<Layer> layers_;

    // 指定导出的区域，默认不按区域导出，若指定了区域，则仅会导出当前 TOP cell 及其后代，并且不会导出空 Cell。不能存在
    // Empty 的区域，否则导出会失败。外部多线程时，需要事先更新 bbox。
    std::vector<BoxI> regions_;

    // 在指定导出区域时，标识是否按区域对图形裁剪(无 merge 效果)。仅在 region_ 非空区域时有效
    bool clip_{false};

    // 导出时,指定的使用的线程数量
    uint32_t thread_num_{1};
};

/**
 * @brief 将 Layout 对象数据导出为 HGS 文件
 * 注意，对同一个 layout 对象进行并行导出时，需要先更新各 Cell 的 Layer 与 BoundingBox 的缓存
 *
 * @param layout 待导出数据的 Layout 对象
 * @param path 导出的 HGS 文件路径
 * @param option 导出的配置参数
 * @return 成功则返回 true，否则返回 false
 */
bool MEDB_API Write(Layout *layout, const std::string &path, const HgsWriteOption &option = {});

/**
 * @brief 将 layout 中的数据导出到指定 std::stringstream 对象中
 * 注意，对同一个 layout 对象进行并行导出时，需要先更新各 Cell 的 Layer 与 BoundingBox 的缓存
 *
 * @param layout 被导出数据的 Layout 对象
 * @param str_stream 存储被导出数据的 std::stringstream 对象
 * @param option 导出 hgs 文件的配置参数
 * @return 成功导出则返回 true, 否则返回 false
 */
bool MEDB_API Write(Layout *layout, std::stringstream &str_stream, const HgsWriteOption &option = {});
}  // namespace hgs
}  // namespace medb2

#endif  // MEDB_HGS_H
