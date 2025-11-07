/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_LAYOUT_H
#define MEDB_LAYOUT_H

#include <string>
#include <vector>

#include "medb/box.h"
#include "medb/cell.h"
#include "medb/errcode.h"
#include "medb/interval.h"
#include "medb/layer.h"
#include "medb/properties.h"

namespace medb2 {

class ThreadPool;
/**
 * @brief 计算面积密度接口的配置参数集对象。
 *
 * 共有 2 种模式的计算方式：kDensityGrid 与 kDensityRandom，通过 type 字段指定；
 * region、stepx、stepy 字段只在 kDensityGrid 模式下生效；
 * polygons 字段只在 kDensityRandom 模式下生效；
 *
 */
class DensityOption {
public:
    /**
     *  @brief 判断当前 DensityOption 对象是否有效。
     *
     *  @return false: 无效； true: 有效
     */
    bool IsValid() const
    {
        if (layers.empty() || (type == DensityType::kDensityGrid && (region.Empty() || stepx <= 0 || stepy <= 0)) ||
            (type == DensityType::kDensityRandom && polygons.empty())) {
            return false;
        }

        return true;
    }

    std::vector<Layer> layers;
    DensityType type{DensityType::kDensityGrid};
    BoxI region;
    int32_t stepx{0};
    int32_t stepy{0};
    std::vector<PolygonI> polygons;
};

class MEDB_API DrcOption {
public:
    explicit DrcOption(const Layer &main_layer) : main_layer_(main_layer) {}

    const Layer &MainLayer() const { return main_layer_; }
    const std::optional<Layer> &RefLayer() const { return ref_layer_; }
    const Interval<double> &Constraint() const { return constraint_; }
    bool DetectE2E() const { return detect_e2e_; }
    bool DetectInternal() const { return detect_internal_; }

    void SetMainLayer(const Layer &main_layer) { main_layer_ = main_layer; }
    void SetRefLayer(const std::optional<Layer> &ref_layer) { ref_layer_ = ref_layer; }
    void SetConstraint(const Interval<double> &constraint) { constraint_ = constraint; }
    void SetDetectE2E(bool detect_e2e) { detect_e2e_ = detect_e2e; }
    void SetDetectInternal(bool detect_internal) { detect_internal_ = detect_internal; }

    bool IsValid() const;
    Interval<uint32_t> IntegerInterval() const;

private:
    Layer main_layer_;                // 主层
    std::optional<Layer> ref_layer_;  // 参考层
    Interval<double> constraint_;     // 区间范围
    bool detect_e2e_{true};           // true 表示检测 e2e，false 表示检测 c2c
    bool detect_internal_{true};      // true 表示检测 internal，false 表示检测 external
};

/**
 * @brief Layout类，对应一个版图文件
 *
 */
class MEDB_API Layout {
public:
    Layout();
    ~Layout();

    /**
     * @brief 获取版图文件dbu
     *
     * @return double 获取到的版图文件dbu
     */
    double GetDatabaseUnit() const { return dbu_; }

    /**
     * @brief 设置版图文件dbu
     *
     * @param value 版图文件的dbu
     *
     * @return 成功返回 MEDB_SUCCESS，失败 MEDB_FAILURE
     */
    MEDB_RESULT SetDatabaseUnit(double value);

    /**
     * @brief 打平之前TopCell包含直属图形和inst，打平过程，就是将inst中的图形，全部变换到TopCell的坐标系
     * 使得原先inst中的图形，变为TopCell的直属图形。
     *
     * @return 成功返回 MEDB_SUCCESS，失败 MEDB_FAILURE
     */
    MEDB_RESULT FlattenTopCell();

    /**
     * @brief 对任意cell进行打平。打平之前Cell包含直属图形和inst，打平过程，就是将inst中的图形，全部变换到Cell的坐标系
     * 使得原先inst中的图形，变为cellCell的直属图形。
     *
     * @param cell_name 待打平的cell的name
     * @param delete_no_used 打平后是否删除不再被使用的子Cell
     *
     * @return 成功返回 MEDB_SUCCESS，失败 MEDB_FAILURE
     */
    MEDB_RESULT FlattenCell(const std::string &cell_name, bool delete_no_used);

    /**
     * @brief 获取TopCell的指针（当前layout仅支持单一的top_cell）
     *
     * @return TopCell的指针,当layout为空或LoadData后没有主动调用Update，该函数返回nullptr
     */
    Cell *TopCell() const;

    /**
     * @brief 删除 Cell 以及它的 Instance 关系
     *
     * @param cell_name 需要删除的 Cell 的Name
     * @param type 支持按 kShallow 或 kDeep 模式删除
     * @return 成功删除则返回 MEDB_SUCCESS，否则返回 MEDB_FAILURE
     */
    MEDB_RESULT DeleteCell(const std::string &cell_name, DeleteCellType type);

    /**
     * @brief 根据入参判断获取获取 layout 中所有的Layers，默认为 true，此函数消耗大量时间,尽可能少使用
     *
     * @param be_used 为 true 获取 top_cell 下的 Layers，否则获取 Layout 的所有 Layers
     * @return 获取到的layer集合
     */
    std::vector<Layer> GetLayers(bool be_used = true) const;

    /**
     * @brief 判断是否存入参layer是否存在, 此函数消耗大量时间,尽可能少使用
     *
     * @param layer 带查找的目标layer对象引用
     * @param be_used 为true时，判断是否在当前TopCell，false时，判断是否在当前layout中
     * @return 存在返回True，否则返回False
     */
    bool HasLayer(const Layer &layer, bool be_used = true) const;

    /**
     * @brief 删除所有cell存在的指定layer，若不存在不做操作
     *
     * @param layer 需要删除的 layer
     */
    MEDB_RESULT RemoveLayer(const Layer &layer);

    /**
     * @brief 在layout中创建一个 layer，若存在返回 MEDB_LAYER_REPEAT
     *
     * @param layer 需要创建的 layer
     */
    MEDB_RESULT CreateLayer(const Layer &layer);

    /**
     * @brief 获取layer上所有的图形的最小包围矩形（外包围盒，不要求先执行flattenTopCell）
     *
     * @param layers 待计算的boundingbox的目标layers集合
     * @return BoxI 目标Layers集合中，所有layer的所有几何图形的BoundingBox
     */
    BoxI GetBoundingBox(const std::vector<Layer> &layers);

    /**
     * @brief 获取layout当前top cell的bounding box（不要求先执行flattenTopCell）
     *
     * @return BoxI layout当前top cell的bounding box
     */
    BoxI GetBoundingBox();

    /**
     * @brief 获取入参layer内所有图形的数量
     * 1.不要求先执行flattenTopCell，
     * 2.其值为polygon数量+box数量+path数量（polygon，box，path本质上都是polygon））
     *
     * @param layer 待计算图形数目的layer
     * @return size_t layer上的几何图形数量
     */
    size_t GetPolygonCount(const Layer &layer);

    /**
     * @brief 新增名为to的layer（如果新增前已存在，则先删除)
     * 将名为from的layer上的图像全部转移到to上，删除旧的Layer from
     * 其行为类似linux文件系统的mv命令。
     *
     * @param from 为移动之前的layer
     * @param to 为移动之后的layer
     * @return MEDB_RESULT MEDB内部定义的错误码
     */
    MEDB_RESULT MoveLayer(const Layer &from, const Layer &to);

    /**
     * @brief 当前TopCell中，对in_layer中的数据做OR操作后结果存放在out_layer中
     *
     * @param in_layer 输入的layer
     * @param out_layer 输出的layer，若已存在则输出会覆盖原数据
     * @return MEDB_RESULT MEDB内部定义的错误码
     */
    MEDB_RESULT LayerBinning(const Layer &in_layer, const Layer &out_layer);

    /**
     * @brief
     * in_layout_a中的layout指定layer与in_layout_b中的layout指定layer进行boolean操作，结果输出到out_layout的layout指定layer中
     *
     *  @param op 要进行的boolean操作
     *  @param in_layout_a 包含layout和layer，指定需要做boolean的layout以及layer
     *  @param in_layout_b 包含layout和layer，指定需要做boolean的layout以及layer
     *  @param out_layout  包含layout和layer，指定boolean后的输出结果
     *  @param optinal  包含window step和thread num，设置boolean参数
     *  @return 1.LAYOUT_LAYER_NOEXIST 两个Layout其中有一个不存在任一指定Layers时，或当前输出layout的layer存在
     *  2. MEDB_FAILURE layout没有topcell 3. MEDB_SUCCESS 执行成功不存在差异
     */
    static MEDB_RESULT DoBoolean(const BooleanType op, const std::pair<const Layout *, Layer> &in_layout_a,
                                 const std::pair<const Layout *, Layer> &in_layout_b,
                                 std::pair<Layout *, Layer> &out_layout);
    /**
     * @brief 当前TopCell中，对in_layers中的数据做op操作后结果存放在out_layer中,
     * 操作流程为in_layers[0]层的图形与其他层图形按顺序做op操作, 例如layers[0] - layers[1] - layers[2] - ... -layers[n]
     *
     * @param op 可以是kOr、kAnd、kSub、kXor
     * @param in_layers 输入的layers, size 必须大于等于 2
     * @param out_layer 输出的layer，若已存在则输出为追加结果
     * @return MEDB_RESULT MEDB内部定义的错误码
     */
    MEDB_RESULT DoBoolean(const BooleanType op, const std::vector<Layer> &in_layers, const Layer &out_layer);

    /**
     * @brief 对输入的in_layers且在cell_name里的图形做op操作后结果存放在out_layer
     *
     * @param cell_name 指定的cell
     * @param op 可以是kOr、kAnd、kSub、kXor
     * @param in_layers 输入的layers, size 必须大于等于 2
     * @param out_layer 输出的layer，若已存在则输出为追加结果
     * @return MEDB_RESULT MEDB内部定义的错误码
     */
    MEDB_RESULT DoBooleanCell(const std::string &cell_name, const BooleanType op, const std::vector<Layer> &in_layers,
                              const Layer &out_layer);

    /**
     * @brief 自身Layout与其他Layout比较指定的不同layers中的图形，结果输出为新的版图文件
     *
     *  @param layout 要比较的Layout, 当layout为nullptr，比较对象为layout自身
     *  @param layers_a 自身layout指定的要比较的layers
     *  @param layers_b 比较layout指定的要比较的layers
     *  @param out_layout  输出layout
     *  @return 1.LAYOUT_LAYER_NOEXIST 两个Layout其中有一个不存在任一指定Layers时
     *  2. MEDB_FAILURE 执行成功但存在差异 3. MEDB_SUCCESS 执行成功不存在差异 4.MEDB_PARAM_ERR
     * 两个比较Layer长度不等时或存在重复Layer
     */
    MEDB_RESULT Compare(Layout *layout, const std::vector<Layer> &layers_a, const std::vector<Layer> &layers_b,
                        Layout &out_layout);

    /**
     * @brief 将 from 层中各多边形的边沿法线方向移动 delta 距离，结果输出到 to 中；delta 可正可负（有方向），
     * 对于外圈，向外是正方向；对于内圈，向内移动是正方向，所以此操作可能会使内圈消失。from 必须以存在，to 必须不存在
     *
     * @param from 被 resize 的 layer，必须在当前 layout 中存在。DoResize 前后，此层数据不变
     * @param to 此操作的输出 layer，必须在当前版图的 top_cell 中不存在
     * @param delta 缩放值
     * @return DATA_RESULT；LAYOUT_LAYER_NOEXIST：from 不存在；
     *                      LAYOUT_ERR_PARA_ERR：to 已经存在了；
     *                      DATA_RESULT_SUCCESS: 执行成功；
     */
    MEDB_RESULT DoResize(const Layer &from, const Layer &to, int delta);

    /**
     * @brief 设置boolean相关操作的线程相关属性
     *
     * @param thread_num 线程数量
     * @param window_step 版图分片计算时，正方形分片区域的边长
     */
    MEDB_RESULT SetGeometryOperationOption(uint32_t thread_num, uint32_t window_step);

    /**
     * @brief 根据cellname获取cell，如果没有则创建
     *
     * @param cell_name 为待查找的cell的name
     * @param create_if_non_exist 为如果查找不到是否创建一个新cell的bool标记
     * @return Cell* 查找到的或新创建的Cell指针
     */
    Cell *GetOrCreateCell(const std::string &cell_name, bool create_if_non_exist = true);

    /**
     * @brief 复制 froms 中的数据到 to
     *
     * @param froms 需要复制的层的容器
     * @param to 目标层
     */
    MEDB_RESULT CloneLayers(const std::vector<Layer> &froms, const Layer &to);

    /**
     * @brief 获取当前Layout中所有的cell的指针
     *
     * @return 表示当前Layout中所有的cell的指针数组
     */
    std::vector<Cell *> GetAllCells();

    /**
     *  @brief 设置当前的TopCell
     *
     *  @param cell_name 要设置的Cell的名称
     */
    MEDB_RESULT SetTopCell(const std::string &cell_name);

    /**
     * @brief 获取所有的 root 名字
     *
     * @return 包含所有 root 名称的容器
     */
    std::vector<const Cell *> GetRootCells() const;

    /**
     * @brief 重命名 cell 的名字
     *
     * @param old_name 要被重命名的 cell 的名字
     * @param new_name cell 的新名字
     * @return 若 old_name 指定的 cell 不存在或已存在 new_name 的 cell 或 new_name 为空字符串则返回 MEDB_FAILURE,
     * 否则成功重命名并返回 MEDB_SUCCESS
     */
    MEDB_RESULT RenameCell(const std::string &old_name, const std::string &new_name);

    /**
     * @brief 将 from 的 cell 的内容合并到 to_name 的 cell 中，并删除 from_name 的 cell。若 to_name 的 cell
     * 不存在，则直接重命名 from_name 为 to_name。若 from_name 中已有父 cell，则会调用失败。
     * 此接口仅在版图文件并行导入的场景使用。
     *
     * @param from_name 被合并 cell 的名字，合并成功后将被删除
     * @param to_name 合并的目标 cell 的名字
     * @return 成功返回 MEDB_SUCCESS，from_name 不存在或其有父cell的话返回 MEDB_FAILURE
     */
    MEDB_RESULT MergeCellForInternalUse(const std::string &from_name, const std::string &to_name);

    /**
     * @brief 设置 Layer 的 name,若 layer 不存在将在 layout 上新增一个 layer 且设置 name
     *
     * @param layer 要设置的 layer
     * @param name 要设置的 layer 的名称
     *
     * @return 返回成功,不会失败
     */
    MEDB_RESULT SetLayerName(const Layer &layer, const std::string &name);

    /**
     * @brief 获取 Layout 的 Properties
     */
    Properties &LayoutProperties();

    /**
     * @brief 获取 Layer 的 name
     *
     * @param layer 要获取名称的 layer
     * @return 当前 layer 的名称,不存在则返回 std::string()
     */
    std::string GetLayerName(const Layer &layer) const;

    /**
     * @brief 合并多个 Layout 到当前 Layout 中。
     * layouts 中的 Cell 若不存在于当前 Layout 中，则将被直接移动过来，若也存在于当前 Layout
     * 中，则移动其图形数据及上下级关系。 若当前 Layout 对象也在 layouts 中，则结果可能非预期。此操作需要确保所有 Layout
     * 的 dbu 是一致的。此操作后，layouts 中所有对象不再可用。
     *
     * @param layouts 需要合并的多个 Layout，此操作后，其中所有的 Layout 对象不再可用
     * @return 返回合并后的 Layout, 也就是调用者自己
     */
    Layout *Merge(const std::vector<Layout *> &layouts);

    /**
     * @brief 更新 Layout 数据，可更新的内容请参考 flag 字段。由用户主动调用
     *
     * @param flag 其含义请参考 const.h 中的 kUpdateXXX 变量，当前支持如下
     *             kUpdateBoundingBox更新boundingbox缓存，只支持只读场景
     *             kUpdateSpatialIndex更新空间索引
     *             kUpdateLayer更新Layer
     *             kUpdateTopCell更新TopCell和RootCell
     */
    MEDB_RESULT Update(uint16_t flag);

    /**
     * @brief 对指定 Cell 上的所有图形坐标按 trans 进行变换。注意，这也会影响其 Sref / Aref 的 Transformation 信息
     *
     * @param cell_name 指定的 Cell 的名称
     * @param trans 用于变换的数据
     */
    MEDB_RESULT TransformCell(const std::string &cell_name, const Transformation &trans);

    /**
     *  @brief 计算 top_cell 及其所有子 cell 中的密度，支持两种模式：
     * kDensityGrid 模式，根据 region 和 step 划分，除不尽则最后一格用余数，每个格子计算密度
     * kDensityRandom 模式，根据 polygons 划分，计算每个 polygon 内的密度
     *
     *
     *  @param option 指定模式和各模式下的参数
     *  @param densities 出参, kDensityGrid 模式, 返回各 layer 上每个格子的密度，先 layer，然后从小到大先 x 后 y;
     *          kDensityRandom 模式，返回各 layer 上每个 polygons 的密度，先 layer，然后按照 polygons 顺序
     *  @return MEDB_RESULT 返回 MEDB_SUCCESS 则表示成功，否则返回其他错误码
     */
    MEDB_RESULT Density(const DensityOption &option, std::vector<double> &densities) const;

    /**
     *  @brief 对 layout 进行压缩，通过压缩等级控制压缩算法及压缩内容
     * ———————————————————————————————————————————————————————————————————
     * | compress_level |                  压缩算法                       |
     * |       0        |                 不进行压缩                      |
     * |       1        |    对 Shapes 中的 Box 应用 VectorCompressAlgo   |
     * |       2        |    对 Shapes 中的 Box 应用 ArrayCompressAlgo    |
     * |       3        |  对 Shapes 中的 Polygon 应用 VectorCompressAlgo |
     * |       4        |  对 Shapes 中的 Polygon 应用 ArrayCompressAlgo  |
     * ———————————————————————————————————————————————————————————————————
     *  @param compress_level 压缩等级
     *  @return MEDB_RESULT 返回 MEDB_SUCCESS
     */
    MEDB_RESULT Compress(uint32_t compress_level);

    /**
     *  @brief 对 layout 进行解压缩
     *
     *  @return MEDB_RESULT 返回 MEDB_SUCCESS
     */
    MEDB_RESULT Decompress();

    /**
     * @brief 获取与指定区域相交的 cell，只作用于当前 top cell 及其子孙 cell
     *
     * @param region 查询区域
     * @param level 查询深度，0 为 当前 top cell
     * @return 与指定区域相交的 cell，无重复元素，无序
     */
    std::vector<Cell *> GetCellsInRegion(const BoxI &region, uint32_t level) const;

    /**
     * @brief 根据输入的 option 指定的方式，检测违规边对
     *
     * @param option 违规检查的模式参数
     * @param edges 出参，表示多对违规的边对
     * @return MEDB_RESULT 返回 MEDB_SUCCESS 则表示成功，否则返回其他错误码
     */
    MEDB_RESULT Drc(const DrcOption &option, std::vector<std::pair<EdgeI, EdgeI>> &edges) const;

private:
    /**
     * @brief 在当前layout中增加一个临时Layer
     *
     * @param temp_layer 输出新增的layer
     * @return true 新增成功
     * @return false 新增失败
     */
    bool GetTempLayer(Layer &temp_layer);

    /**
     * @brief 计算 kDensityGrid 模式下各个网格的面积密度
     *
     * @param layer 待计算面积的图形所在的 layer
     * @param box 待划分网格的区域
     * @param stepx 划分网格的 x 轴步进
     * @param stepy 划分网格的 y 轴步进
     * @return std::vector<double> 每个网格的面积密度集
     */
    std::vector<double> DensityImpl(const Layer &layer, const BoxI &box, int stepx, int stepy) const;

    /**
     * @brief 计算 kDensityRandom 模式下各个网格的面积密度
     *
     * @param layer 待计算面积的图形所在的 layer
     * @param polygons 计算面积密度的多边形区域集
     * @return std::vector<double> 每个多边形区域的面积密度集
     */
    std::vector<double> DensityImpl(const Layer &layer, const std::vector<PolygonI> &polygons) const;

    /**
     * @brief 移动 layout 的数据，若目标 cell_pool 不存在这个 cell，则直接移动cell，否则移动 cell 下的 shapes
     *
     * @param it cell_pool 的迭代器
     * @param sub_cell_pool 所需要移动的 list
     */
    void MergeImpl(MeList<Cell>::iterator &it, MeList<Cell> &sub_cell_pool);

    /**
     * @brief 多线程更新 layer_name_map
     *
     * @param cell_pool 入参 cell_pool
     * @param thread_num 可用的线程数
     * @param layer_name_map 出参，表示 layer 和 name 的映射
     */
    void MultiThreadUpdateLayer(const MeList<Cell> &cell_pool, std::unique_ptr<ThreadPool> &thread_pool,
                                uint32_t thread_num, MeUnorderedMap<Layer, std::string> &layer_name_map);

    /**
     * @brief 单线程更新 layer_name_map
     *
     * @param cell_pool 入参 cell_pool
     * @param layer_name_map 出参，表示 layer 和 name 的映射
     */
    void SingleThreadUpdateLayer(const MeList<Cell> &cell_pool, MeUnorderedMap<Layer, std::string> &layer_name_map);

    void UpdateBoundingBox(std::unique_ptr<ThreadPool> &thread_pool);

    void UpdateSpatialIndex(std::unique_ptr<ThreadPool> &thread_pool);

    /**
     * @brief 以 kDeep 模式删除指定的 Cell，此模式会递归地删除不再被使用的子 Cell
     *
     * @param cell 待删除的 Cell
     */
    void DeleteCellDeeply(Cell &cell);

private:
    double dbu_{0.0};
    MeList<Cell> cell_pool_;
    mutable Cell *top_cell_{nullptr};
    MeUnorderedMap<std::string, MeList<Cell>::iterator> cell_name_map_;
    MeUnorderedMap<Layer, std::string> layer_name_map_;
    Properties layout_properties_;

    uint32_t thread_num_{kDefaultThreadNum};
    uint32_t window_step_{kDefaultWindowStep};
};
}  // namespace medb2
#endif