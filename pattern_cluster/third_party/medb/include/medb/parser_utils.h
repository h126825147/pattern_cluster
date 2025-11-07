/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_PARSER_UTILS_H
#define MEDB_PARSER_UTILS_H

#include <string>
#include "medb/allocator.h"
#include "medb/box.h"
#include "medb/layer.h"

namespace medb2 {
/**
 * @brief 版图导入时的回调函数类型定义
 *
 * @param percent 版图导入的进度信息，取值为[0, 100]
 */
using ReadProgressCallback = void (*)(int percent);

/**
 * @brief DDE流程结果回调的Tile相关数据，如tile的box范围，被切分的oas的二进制数据（可能有多个oas）等。
 */
class DdeTileData {
public:
    const BoxI &TileRegion() const { return tile_region_; }
    void SetTileRegion(const BoxI &tile_region) { tile_region_ = tile_region; }

    const std::vector<std::pair<const char *, size_t>> &OasData() const { return oas_data_; }
    void AddOasData(const char *oas_bytes, size_t oas_bytes_len) { oas_data_.emplace_back(oas_bytes, oas_bytes_len); }

private:
    BoxI tile_region_;
    std::vector<std::pair<const char *, size_t>> oas_data_;
};

/**
 * @brief DDE流程Tile子区域oasis版图的回调定义
 *
 * @param context 用户设置的自定义类型的指针，内部只透传，不解析使用
 *
 * @param dde_tile_data tile分块区域的相关数据，如tile的box范围，被切分的oas的二进制数据（可能有多个oas）
 */
using TileDdeCallback = void (*)(void *context, const DdeTileData &dde_tile_data);

class DdeLibOption {
public:
    void SetFilePath(const std::string &file_path) { file_path_ = file_path; }
    const std::string &FilePath() const { return file_path_; }

    void SetLayers(const std::vector<Layer> &layers) { layers_ = layers; }
    const std::vector<Layer> &Layers() const { return layers_; }

    void SetTopCellName(const std::string &top_cell_name) { top_cell_name_ = top_cell_name; }
    const std::string &TopCellName() const { return top_cell_name_; }

    USE_MEDB_OPERATOR_NEW_DELETE
private:
    std::string file_path_;
    std::vector<Layer> layers_;
    std::string top_cell_name_;
};

class DdeImplOption {
public:
    void SetTileWidth(double tile_width) { tile_width_ = tile_width; }
    double TileWidth() const { return tile_width_; }

    void SetTileHeight(double tile_height) { tile_height_ = tile_height; }
    double TileHeight() const { return tile_height_; }

    void SetAmbit(double ambit) { ambit_ = ambit; }
    double Ambit() const { return ambit_; }

    void SetDdeCallback(TileDdeCallback dde_callback) { dde_callback_ = dde_callback; }
    TileDdeCallback DdeCallback() const { return dde_callback_; }

    void SetThreadNum(uint32_t thread_num) { thread_num_ = thread_num; }
    uint32_t ThreadNum() const { return std::max(thread_num_, 1U); }

    void SetDdeContext(void *dde_context) { dde_context_ = dde_context; }
    void *DdeContext() const { return dde_context_; }

    USE_MEDB_OPERATOR_NEW_DELETE
private:
    double tile_width_{0.0};
    double tile_height_{0.0};
    double ambit_{0.0};
    uint32_t thread_num_{0};
    TileDdeCallback dde_callback_{nullptr};
    void *dde_context_{nullptr};
};

/**
 * @brief DDE流程导入并分发使用的配置参数
 */
class DdeOption {
public:
    void SetLibOptions(const std::vector<DdeLibOption> &lib_opt) { lib_opt_ = lib_opt; }
    const std::vector<DdeLibOption> &LibOptions() const { return lib_opt_; }

    void SetImplOption(const DdeImplOption &impl_opt) { impl_opt_ = impl_opt; }
    const DdeImplOption &ImplOption() const { return impl_opt_; }

    USE_MEDB_OPERATOR_NEW_DELETE
private:
    std::vector<DdeLibOption> lib_opt_;
    DdeImplOption impl_opt_;
};
}  // namespace medb2
#endif