/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_GOA_H
#define MEDB_GOA_H

#include <vector>
#include "medb/base_utils.h"
#include "medb/box.h"
#include "medb/point.h"

namespace medb2 {
namespace goa {

/**
 * @brief 缩放模式
 */
enum ResizeMode : std::uint8_t {
    kResizeDefault = 0,
    kOverUnder,
    kUnderOver,
};

/**
 * @brief 操作参数，包含四组方向和偏移量
 *        sequence存储的值必须包含0/1/2/3四个值，分别对应右/上/左/下，顺序不限
 *        values依次存储右/上/左/下的偏移量
 */
struct GrowOrShrinkParam {
    std::uint8_t sequences[4];
    std::int64_t values[4];
};

/**
 * @brief 区间表示
 * @param lower_bound 左区间
 * @param upper_bound 右区间
 * @param lower_open 左区间是否为开区间
 * @param upper_open 右区间是否为开区间
 */
template <class T>
struct LengthCondition {
    T lower_bound;
    T upper_bound;
    bool lower_open = true;  // true 表示为开区间
    bool upper_open = true;  // true 表示为开区间
};

/**
 * @brief 筛选洞的参数
 * @param do_constraint 是否有面积约束
 * @param constraint 面积区间
 * @param not_equal true表示不等于constraint.lower_bound
 * @param inner true表示输出最内层的hole，false表示输出最外层的hole
 * @param empty true表示hole内不能存在任何polygon，false表示无限制
 * @param binning 没用到，原意为是否将输出的Hole做一次binning
 */
struct GetHoleInPolygonsParams {
    bool do_constraint{false};
    LengthCondition<double> constraint;
    bool not_equal{false};
    bool inner{false};
    bool empty{false};
    bool binning{true};
};

/**
 * @brief 多个点集表示的多边形集合进行布尔运算
 *
 * 对 polygon_a 与 polygon_b 中的点集形式的多边形集合进行布尔运算，这里的运算类型包含 或、与、差、异或。
 *

 * @param op 指定布尔运算类型，可用的类型有 或、与、差、异或
 * @param polygon_a 入参，点集形式表示的多边形集合, PolygonPtrDataI
 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param polygon_b 入参，点集形式表示的多边形集合, PolygonPtrDataI
 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param polygon_out 出参，点集形式表示的多边形集合，输出时同样是外顺内逆
 */
void MEDB_API Boolean(const BooleanType op, const std::vector<PolygonPtrDataI> &polygon_a,
                      const std::vector<PolygonPtrDataI> &polygon_b, std::vector<PolygonDataI> &polygon_out);

/**
 * @brief 多个点集表示的曼哈顿多边形集合进行布尔运算
 *
 * 对 polygon_a 与 polygon_b 中的点集形式的多边形集合进行布尔运算，这里的运算类型包含 或、与、差、异或。
 *

 * @param op 指定布尔运算类型，可用的类型有 或、与、差、异或
 * @param polygon_a 入参，点集形式表示的多边形集合, PolygonPtrDataI
 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param polygon_b 入参，点集形式表示的多边形集合, PolygonPtrDataI
 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param compress_type 压缩类型, 指的是polygon_a和polygon_b,以及polygon_out的压缩方式
 * @param polygon_out 出参，点集形式表示的多边形集合，输出时同样是外顺内逆
 */
void MEDB_API BooleanManhattan(const BooleanType op, const std::vector<PolygonPtrDataI> &polygon_a,
                               const std::vector<PolygonPtrDataI> &polygon_b, ManhattanCompressType compress_type,
                               std::vector<PolygonDataI> &polygon_out);

/**
 * @brief 多个点集表示的半曼哈顿多边形集合进行布尔运算
 *
 * 对 polygon_a 与 polygon_b 中的点集形式的多边形集合进行布尔运算，这里的运算类型包含 或、与、差、异或。
 *

 * @param op 指定布尔运算类型，可用的类型有 或、与、差、异或
 * @param polygon_a 入参，点集形式表示的多边形集合, PolygonPtrDataI
 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param polygon_b 入参，点集形式表示的多边形集合, PolygonPtrDataI
 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param polygon_out 出参，点集形式表示的多边形集合，输出时同样是外顺内逆
 */
void MEDB_API BooleanOctangular(const BooleanType op, const std::vector<PolygonPtrDataI> &polygon_a,
                                const std::vector<PolygonPtrDataI> &polygon_b, std::vector<PolygonDataI> &polygon_out);

/**
 * @brief 多个点集表示的任意角度多边形集合进行布尔运算
 *
 * 对 polygon_a 与 polygon_b 中的点集形式的多边形集合进行布尔运算，这里的运算类型包含 或、与、差、异或。
 *

 * @param op 指定布尔运算类型，可用的类型有 或、与、差、异或
 * @param polygon_a 入参，点集形式表示的多边形集合, PolygonPtrDataI
 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param polygon_b 入参，点集形式表示的多边形集合, PolygonPtrDataI
 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param polygon_out 出参，点集形式表示的多边形集合，输出时同样是外顺内逆
 */
void MEDB_API BooleanAnyAngle(const BooleanType op, const std::vector<PolygonPtrDataI> &polygon_a,
                              const std::vector<PolygonPtrDataI> &polygon_b, std::vector<PolygonDataI> &polygon_out);
/**
 * @brief 多边形布尔与运算
 *
 * 计算 polygons 集合中所有多边形与 box 相交的结果，也即重叠的区域。这里不关心 polygons
 * 集合中的元素会不会做两两相交运算，重叠的区域一定在 box 中。
 *
 * @param polygons 入参，点集形式表示的多边形集合, PolygonPtrDataI
 * 中第一个表示外环需要是顺时针，其他的内环需要是逆时针，输入点集不能有共线点或重复点
 * @param box 入参，UserBox 对象，本接口计算后的重叠区域一定是在此 box 中
 * @param out_group 出参，polygons 与 box 相交的结果集
 * @return true polygons 与 box 有重叠区域
 * @return false polygons 与 box 没有重叠区域
 */
bool MEDB_API Intersect(const std::vector<PolygonPtrDataI> &polygons, const BoxI &box,
                        std::vector<PolygonDataI> &out_group);

/**
 * @brief 多边形布尔或运算，计算多个点集形式表示的多边形的并集。若图形个数为 1 或者因为图形自相交导致 BooleanOR
 * 的结果为空，则会直接返回原始点集。
 *
 * 计算给定的多个多边形的并集
 *
 * @param source_group 入参，待求并集的点集形式表示的多边形集合
 * @param out_group 出参，入参多边形的并集
 */
void MEDB_API BooleanORForInternalUse(const std::vector<PolygonPtrDataI> &source_group,
                                      std::vector<PolygonDataI> &out_group);

/**
 * @brief 多边形布尔或运算，计算多个点集形式表示的多边形的并集
 *
 * 计算给定的多个多边形的并集
 *
 * @param source_group 入参，待求并集的点集形式表示的多边形集合
 * @param out_group 出参，入参多边形的并集
 */
void MEDB_API BooleanOR(const std::vector<PolygonPtrDataI> &source_group, std::vector<PolygonDataI> &out_group);

/**
 * @brief 曼哈顿多边形布尔或运算，计算多个点集形式表示的多边形的并集
 *
 * 计算给定的多个多边形的并集
 *
 * @param source_group 入参，待求并集的点集形式表示的多边形集合
 * @param compress_type 压缩类型
 * @param out_group 出参，入参多边形的并集
 */
void MEDB_API BooleanORManhattan(const std::vector<PolygonPtrDataI> &source_group, ManhattanCompressType compress_type,
                                 std::vector<PolygonDataI> &out_group);

/**
 * @brief 半曼哈顿多边形布尔或运算，计算多个点集形式表示的多边形的并集
 *
 * 计算给定的多个多边形的并集
 *
 * @param source_group 入参，待求并集的点集形式表示的多边形集合
 * @param out_group 出参，入参多边形的并集
 */
void MEDB_API BooleanOROctangular(const std::vector<PolygonPtrDataI> &source_group,
                                  std::vector<PolygonDataI> &out_group);

/**
 * @brief 任意角度多边形布尔或运算，计算多个点集形式表示的多边形的并集
 *
 * 计算给定的多个多边形的并集
 *
 * @param source_group 入参，待求并集的点集形式表示的多边形集合
 * @param out_group 出参，入参多边形的并集
 */
void MEDB_API BooleanORAnyAngle(const std::vector<PolygonPtrDataI> &source_group, std::vector<PolygonDataI> &out_group);
/**
 * @brief 调整任意角度多边形大小
 *
 * 对指定多边形集合中的每个多边形的每条边按指定距离进行移动，以调整每个多边形的大小。
 * 调整完大小后，还需要对集合中的多边形进行合并操作（BooleanType::OR）。
 * 调整大小的规则为：
 *    1. 若参数 size_value 值为正，则多边形的外环的边向外移动 size_value 的距离，多边形中内环（洞）上的边向内移动
 * size_value 的距离
 *    2. 若参数 size_value 值为负，则多边形的外环的边向内移动 size_value 的距离，多边形中内环（洞）上的边向外移动
 * size_value 的距离
 *    3. 若参数 size_value 值为 0，则不进行大小调整
 * 例如：
 *    1. 多边形的点集为 {(0, 0), (1, 0), (1, 1), (1, 0)}，若 size_value = 1，则调整大小后的多边形为 {(-1, -1), (2,
 * -1), (2, 2,), (-1, 2)}。
 *    2. 多边形的点集为 {(0, 0), (1, 0), (1, 1), (1, 0)}，若 size_value = -1，则调整大小后的多边形消失了。
 *
 * @param source_group 入参，要进行调整大小的多边形点集
 * @param size_value 在调整大小时，边的移动距离
 * @param mode 入参, 放大的模式
 *              - kResizeDefault
 *                  - size_value > 0，表示正常放大
 *                  - size_value < 0，表示正常缩小
 *              - kOverUnder, 表示先放大后缩小, size_value必须大于0
 *              - kUnderOver, 表示先缩小后放大, size_value必须大于0
 * @param out_group 出参，调整大小后的多边形点集
 */
void MEDB_API Resize(const std::vector<PolygonPtrDataI> &source_group, int size_value, ResizeMode mode,
                     std::vector<PolygonDataI> &out_group);

/**
 * @brief 多边形进行grow/shrink操作函数(对外接口)
 * @param source_group 入参 输入的多边形指针集合
 * @param param 偏移操作
 * @param out_group 出参 偏移操作后的多边形集合
 */
void MEDB_API SequentialGrowOrShrink(const std::vector<PolygonPtrDataI> &source_group, const GrowOrShrinkParam &param,
                                     std::vector<PolygonDataI> &out_group);

/**
 * @brief HoleOperation
 *
 * 入参为 source_group 和 param
 * 将满足条件的 Hole 筛选出来，放到 out_group 中
 *
 * @param source_group 入参，表示输入的带洞的 Polygon 集合
 * @param param 入参，筛选 Hole 的参数
 * @param out_group 出参，表示筛选出来的 Hole
 */
void MEDB_API Hole(const std::vector<PolygonPtrDataI> &source_group, const GetHoleInPolygonsParams &param,
                   std::vector<PolygonDataI> &out_group);

/**
 * @brief poly 若带洞，则转成自接触的点集
 *
 * @param poly UserPolygon 类型
 */
std::vector<std::vector<std::pair<double, double>>> MEDB_API RingsToSelfTouching(const PolygonPtrDataI &poly);

/**
 * @brief PolygonFracture，将输入的图形分解成多个长方形构成的集合
 *
 * @param manhattan_group 入参，表示输入曼哈顿图形的 Polygon 集合, 必须是曼哈顿图形的集合，必须保证不同的 Polygon
 * 之间不相交
 * @param out_boxes 出参，表示分解出来的长方形集合
 */
void MEDB_API FractureManhattanPolygon(const std::vector<PolygonPtrDataI> &manhattan_group,
                                       std::vector<BoxI> &out_boxes);

}  // namespace goa
}  // namespace medb2
#endif