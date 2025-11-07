/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_CACHE_FILE_H
#define MEDB_CACHE_FILE_H

#include <array>
#include <filesystem>
#include <fstream>
#include "medb/base_utils.h"
#include "medb/box.h"
namespace medb2 {

/**
 * @brief 描述对 oasis 文件进行分割的网格
 */
struct CacheGridParam {
    // 最左下角网格节点的左下角点的位置
    int32_t start_x_{0};
    int32_t start_y_{0};
    // 相邻网格节点的左下角点的偏移量
    uint32_t step_x_{0};
    uint32_t step_y_{0};
    // 网格节点的实际宽、高
    uint32_t width_{0};
    uint32_t height_{0};
    // 分隔出来的行数和列数
    uint32_t rows_{0};
    uint32_t columns_{0};

    CacheGridParam() = default;

    CacheGridParam(const std::pair<int32_t, int32_t> &start, const std::pair<uint32_t, uint32_t> &step,
                   const std::pair<uint32_t, uint32_t> &grid_size, const std::pair<uint32_t, uint32_t> &grid_dimension)
        : start_x_(start.first),
          start_y_(start.second),
          step_x_(step.first),
          step_y_(step.second),
          width_(grid_size.first),
          height_(grid_size.second),
          rows_(grid_dimension.first),
          columns_(grid_dimension.second)
    {
    }

    /**
     * @brief 用全局范围，格网单元格的宽高，以及外扩距离来构造格网模型
     *
     * @param world_box 格网的全局范围
     * @param tile_width 格网单元格的宽度
     * @param tile_height 格网单元格的高度
     * @param tile_ambit 格网单元格的外扩距离（即单元格重叠宽度的一半）
     */
    CacheGridParam(const BoxI &world_box, uint32_t tile_width, uint32_t tile_height, uint32_t tile_ambit)
    {
        start_x_ = world_box.Left();
        start_y_ = world_box.Bottom();
        width_ = tile_width + 2 * tile_ambit;
        height_ = tile_height + 2 * tile_ambit;
        step_x_ = tile_width;
        step_y_ = tile_height;
        rows_ = static_cast<uint32_t>((static_cast<uint64_t>(world_box.Height()) + tile_height - 1U) / tile_height);
        columns_ = static_cast<uint32_t>((static_cast<uint64_t>(world_box.Width()) + tile_width - 1U) / tile_width);
    }

    /**
     * @brief CacheGridParam 的 == 运算符重载 判断两个 CacheGridParam 是否相等
     *
     * @param other 与之判断相等的 CacheGridParam 对象引用
     * @return 相等返回 true, 不相等返回 false
     */
    bool operator==(const CacheGridParam &other) const
    {
        return start_x_ == other.start_x_ && start_y_ == other.start_y_ && step_x_ == other.step_x_ &&
               step_y_ == other.step_y_ && width_ == other.width_ && height_ == other.height_ && rows_ == other.rows_ &&
               columns_ == other.columns_;
    }

    /**
     * @brief 计算region相关（相交）的网格行列范围
     *
     * @param region 目标区域
     * @return 行列的起止范围，左闭右开。[0]:起始行，[1]:起始列，[2]:终止行，[3]:终止列
     */
    std::array<uint32_t, 4> GetRelatedSpan(const BoxI &region) const
    {
        double temp_start_x = start_x_;
        double temp_start_y = start_y_;

        auto limit_func = [](double x, uint32_t max) {
            return x < 0.0 ? 0u : (x > max ? max : static_cast<uint32_t>(x));
        };

        uint32_t start_row = limit_func((-temp_start_y + region.Bottom() - height_) / step_y_ + 1, rows_);
        uint32_t start_col = limit_func((-temp_start_x + region.Left() - width_) / step_x_ + 1, columns_);
        uint32_t end_row = limit_func(std::ceil((region.Top() - temp_start_y) / step_y_), rows_);
        uint32_t end_col = limit_func(std::ceil((region.Right() - temp_start_x) / step_x_), columns_);

        return {start_row, start_col, end_row, end_col};
    }

    /**
     * @brief 获取irow行，icol列的网格单元格的box
     *
     * @param row 目标网格单元格的行号
     * @param col 目标网格单元格的列号
     * @return 目标网格单元格的box范围
     */
    BoxI GetGridCell(uint32_t row, uint32_t col)
    {
        PointI begin_point(Accumulate(start_x_, col * step_x_), Accumulate(start_y_, row * step_y_));
        PointI end_point(Accumulate(begin_point.X(), width_), Accumulate(begin_point.Y(), height_));
        return BoxI(begin_point, end_point);
    }
};

/**
 * @brief 描述 cache 文件的主要数据
 */
struct CacheFileOption {
    // cache 文件的路径
    std::string cache_file_path_;
    // 预处理前的 oasis 文件的路径
    std::string origin_oasis_file_path_;
    // 预处理后的 oasis 文件的路径
    std::string processed_oasis_file_path_;
    // 预处理后 oasis 文件中 TopCell 的offset
    uint64_t top_cell_offset_{0};
    // 网格参数
    CacheGridParam param_;

    /**
     * @brief CacheFileOption 的 == 运算符重载 判断两个 CacheFileOption 是否相等
     *
     * @param other 与之判断相等的 CacheFileOption 对象引用
     * @return 相等返回 true, 不相等返回 false
     */
    bool operator==(const CacheFileOption &other) const
    {
        return cache_file_path_ == other.cache_file_path_ && origin_oasis_file_path_ == other.origin_oasis_file_path_ &&
               processed_oasis_file_path_ == other.processed_oasis_file_path_ &&
               top_cell_offset_ == other.top_cell_offset_ && param_ == other.param_;
    }
};

/**
 * @brief 支持 cache 文件的写入
 */
class MEDB_API CacheFileWriter {
public:
    CacheFileWriter() = default;
    ~CacheFileWriter() = default;

    /**
     * @brief 按 cache 文件格式写入相应数据
     *
     * @param cache_file_option cache 文件的参数
     * @param offsets offsets 各个网格在 oasis 文件中的 offset 数据
     * @return bool 返回 true 表示各个参数都通过了校验，并且数据均已写入了 cache 文件中
     */
    static bool WriteCacheFile(const CacheFileOption &cache_file_option, const std::vector<uint64_t> &offsets);

private:
    /**
     * @brief 根据提供的 oasis 文件路径，读取它的最后修改时间
     *
     * @param oasis_file_path oasis 文件路径
     * @param timestamp 出参，返回 oasis 文件的最后修改时间
     * @return 读取成功返回 true, 否则返回 false
     */
    static bool GetOasisLastWriteTime(const std::string &oasis_file_path, int64_t &timestamp);
};

/**
 * @brief 支持 cache 文件的读取、region查询以及关闭
 */
class MEDB_API CacheFileReader {
public:
    CacheFileReader() = default;
    ~CacheFileReader() = default;

    /**
     * @brief 打开 cache 文件流，按 cache 文件格式，读取 oasis
     * 文件时间戳，CacheFileOption，END标识符等，对文件内容及完整性进行验证
     *
     * @param cache_file_path cache 文件的路径
     * @return bool 返回 true 表示文件流成功打开且文件头已校验通过
     */
    bool BeginFile(const std::string &cache_file_path);

    /**
     * @brief 关闭 cache 文件流
     *
     * @return bool 返回 true 表示文件流成功关闭，返回 false 表示文件流未打开
     */
    bool EndFile();

    /**
     * @brief 根据用户输入的 region 和已读入的 CacheGridParam 计算出与 region 重叠的网格块，并返回对应的 offsets
     *
     * @param region 需要读取的版图的范围
     * @param offsets 出参，表示与 region 重叠的网格块在 oasis 文件中的偏移量
     * @return bool 返回 true 表示成功从cache文件中读取到 offsets
     */
    bool GetCellsInRegion(const BoxI &region, std::vector<uint64_t> &offsets);

    /**
     * @brief 获取 CacheFileReader 对象中的 CacheFileOption
     *
     * @return 返回 option_ 的常量引用
     */
    const CacheFileOption &Option() { return option_; }

private:
    /**
     * @brief 在文件流读到 oasis 参数的起始位置时，从文件流中读取 oasis 文件路径并校验该文件的最后修改时间
     *
     * @param out_oasis_file_path 出参，返回从 cache 文件中读取出来的 oasis 文件路径
     * @return 读取并校验成功返回 true, 否则返回 false
     */
    bool ReadAndCheckOasisFileParam(std::string &out_oasis_file_path);

private:
    // cache 文件流
    std::ifstream ifs_;
    // cache 文件中 offsets 的开始位置相对于文件开头的偏移量
    uint64_t offsets_begin_pos_{0};
    // cache 文件的大小（字节）
    uint64_t cache_file_size_{0};
    // 读出来的 cache 文件参数
    CacheFileOption option_;
};

}  // namespace medb2
#endif
