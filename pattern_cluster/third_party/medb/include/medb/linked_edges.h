/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_LINKED_EDGES_H
#define MEDB_LINKED_EDGES_H

#include <functional>
#include "medb/edge.h"
#include "medb/enums.h"
#include "medb/errcode.h"
#include "medb/point.h"

namespace medb2 {

/**
 * @brief 由多边形的边构成的边链表结构类，用来实现边的打断与移动操作。
 */
template <class CoordType>
class MEDB_API LinkedEdges {
public:
    LinkedEdges() = default;
    ~LinkedEdges() = default;

    /**
     * @brief 通过一个点集数据来构造linkededges对象
     *
     * @param points 表示一条路径的有序点集，路径开（即首尾点不重复）
     * @param is_clockwise 多边形外环的边是否为顺时针方向
     */
    LinkedEdges(const std::vector<Point<CoordType>> &points, bool is_clockwise)
    {
        InitList(points);
        is_clockwise_ = is_clockwise;
    }

    /**
     * @brief 获取linkededges对象的list数据
     *
     * @return 返回edge_list_数据
     */
    MeList<Edge<CoordType>> &EdgeList() { return edge_list_; }

    /**
     * @brief 获取linkededges对象的list数据
     *
     * @return 返回edge_list_数据
     */
    const MeList<Edge<CoordType>> &EdgeList() const { return edge_list_; }

    /**
     * @brief 指定pos位置的edge进行打断
     *
     * @param pos 待打断的目标边迭代器
     * @param distances
     * 以pos位置所在的Edge起点开始，按照distances列表的数据间隔进行打断，支持打多段，且打断的距离可以是浮点型，以保证可扩展性
     *
     * @return 打段操作返回码
     */
    MEDB_RESULT Fragment(typename MeList<Edge<CoordType>>::iterator &pos, MeVector<double> &distances)
    {
        if (distances.empty() || pos == edge_list_.end()) {
            // 不需要打断.
            return MEDB_SUCCESS;
        }
        if (pos->Angle() == kOtherAngle) {
            // 不支持.
            return MEDB_SUCCESS;
        }
        double sum = 0;
        for (auto d : distances) {
            if (DoubleEqual(d, 0.0)) {
                return MEDB_FAILURE;
            }
            sum += d;
        }
        auto edge_len = pos->Length();
        if (DoubleGreaterEqual(sum, edge_len)) {
            return MEDB_FAILURE;
        }
        Point<CoordType> end_point;
        for (auto d : distances) {
            GetPointByDistance(pos, d, end_point);
            Edge<CoordType> edge(pos->BeginPoint(), end_point);
            pos->Set(end_point, pos->EndPoint());
            edge_list_.insert(pos, edge);
        }
        return MEDB_SUCCESS;
    }

    /**
     * @brief 指定pos位置的edge进行移动
     *
     * @param pos 待打断的目标边迭代器
     * @param distance 以pos位置所在的Edge移动量，类型为double以保证可扩展性。
     *
     * @return 打段操作返回码
     */
    MEDB_RESULT Move(typename MeList<Edge<CoordType>>::iterator &pos, double distance)
    {
        if (DoubleEqual(distance, 0.0) || pos == edge_list_.end()) {
            // 不需要移动.
            return MEDB_SUCCESS;
        }
        auto cur_angle = pos->Angle();
        if (cur_angle == kOtherAngle) {
            // 不支持.
            return MEDB_SUCCESS;
        }

        auto pre = pos;
        if (!FindDiffAngelEdge(pos, false, cur_angle, pre)) {
            // 非法的polygon（全部点集共线）.
            return MEDB_FAILURE;
        }

        auto next = pos;
        if (!FindDiffAngelEdge(pos, true, cur_angle, next)) {
            // 非法的polygon（全部点集共线）.
            return MEDB_FAILURE;
        }

        double move_range = std::min(pre->Length(), next->Length());
        if (DoubleGreater(abs(distance), move_range)) {
            return MEDB_FAILURE;
        }

        distance = is_clockwise_ ? distance : -distance;

        Point<CoordType> moved_begin;
        Point<CoordType> moved_end;
        ModifyEdgeVertex(pos, distance, cur_angle, moved_begin, moved_end);
        InsertNewEdges(pos, cur_angle, moved_begin, moved_end);
        return MEDB_SUCCESS;
    }

private:
    // 初始化linkededges对象的list数据，可供构造函数调用
    void InitList(const std::vector<Point<CoordType>> &points)
    {
        if (points.size() < 3) {
            return;
        }
        edge_list_.clear();
        size_t i = 1;
        for (; i < points.size(); ++i) {
            Edge<CoordType> edge(points[i - 1], points[i]);
            edge_list_.insert(edge_list_.end(), edge);
        }
        Edge<CoordType> last_edge(points[i - 1], points[0]);
        edge_list_.insert(edge_list_.end(), last_edge);
    }

    void GetPointByDistance(const typename MeList<Edge<CoordType>>::iterator &pos, double distance,
                            Point<CoordType> &end_point)
    {
        double x = pos->BeginPoint().X();
        double y = pos->BeginPoint().Y();
        switch (pos->Angle()) {
            case kDegree0:
                x = x + distance;
                break;
            case kDegree180:
                x = x - distance;
                break;
            case kDegree90:
                y = y + distance;
                break;
            case kDegree270:
                y = y - distance;
                break;
            default:
                break;
        }

        if constexpr (std::is_same<CoordType, int32_t>::value) {
            end_point.SetX(CoordCvt<CoordType, double>(x));
            end_point.SetY(CoordCvt<CoordType, double>(y));
        } else {
            end_point.SetX(x);
            end_point.SetY(y);
        }
    }

    bool FindDiffAngelEdge(const typename MeList<Edge<CoordType>>::iterator &pos, bool is_forward, AngleType cur_angle,
                           typename MeList<Edge<CoordType>>::iterator &it)
    {
        bool find = false;

        std::function<void(typename MeList<Edge<CoordType>>::iterator &)> move_iter;
        if (is_forward) {
            move_iter = [](auto &iter) { ++iter; };
        } else {
            move_iter = [](auto &iter) { --iter; };
        }
        do {
            if (it == edge_list_.begin() && !is_forward) {
                it = edge_list_.end();
            }
            move_iter(it);
            if (it == edge_list_.end()) {
                it = edge_list_.begin();
            }
            if (it->Angle() != cur_angle) {
                find = true;
                break;
            }
        } while (it != pos);

        return find;
    }

    void ModifyEdgeVertex(typename MeList<Edge<CoordType>>::iterator &pos, double distance, AngleType cur_angle,
                          Point<CoordType> &moved_begin, Point<CoordType> &moved_end)
    {
        double x0 = pos->BeginPoint().X();
        double y0 = pos->BeginPoint().Y();
        double x1 = pos->EndPoint().X();
        double y1 = pos->EndPoint().Y();
        switch (cur_angle) {
            case kDegree0:
                y0 += distance;
                y1 += distance;
                break;
            case kDegree180:
                y0 -= distance;
                y1 -= distance;
                break;
            case kDegree90:
                x0 -= distance;
                x1 -= distance;
                break;
            case kDegree270:
                x0 += distance;
                x1 += distance;
                break;
            default:
                break;
        }
        if constexpr (std::is_same<CoordType, int32_t>::value) {
            moved_begin.SetX(CoordCvt<CoordType, double>(x0));
            moved_begin.SetY(CoordCvt<CoordType, double>(y0));
            moved_end.SetX(CoordCvt<CoordType, double>(x1));
            moved_end.SetY(CoordCvt<CoordType, double>(y1));
        } else {
            moved_begin.SetX(x0);
            moved_begin.SetY(y0);
            moved_end.SetX(x1);
            moved_end.SetY(y1);
        }
        pos->Set(moved_begin, moved_end);
    }

    void InsertNewEdges(const typename MeList<Edge<CoordType>>::iterator &pos, AngleType cur_angle,
                        const Point<CoordType> &moved_begin, const Point<CoordType> &moved_end)
    {
        auto pre_one = pos;
        if (pos == edge_list_.begin()) {
            pre_one = edge_list_.end();
        }
        --pre_one;

        auto next_one = pos;
        ++next_one;
        if (next_one == edge_list_.end()) {
            next_one = edge_list_.begin();
        }

        if (pre_one->Angle() != cur_angle) {
            pre_one->Set(pre_one->BeginPoint(), moved_begin);
        } else {
            Edge<CoordType> edge(pre_one->EndPoint(), moved_begin);
            edge_list_.insert(pos, edge);
        }

        if (pre_one->BeginPoint() == pre_one->EndPoint()) {
            edge_list_.erase(pre_one);
        }

        if (next_one->Angle() != cur_angle) {
            next_one->Set(moved_end, next_one->EndPoint());
        } else {
            Edge<CoordType> edge = Edge<CoordType>(moved_end, next_one->BeginPoint());
            edge_list_.insert(next_one, edge);
        }

        if (next_one->BeginPoint() == next_one->EndPoint()) {
            edge_list_.erase(next_one);
        }
    }

private:
    MeList<Edge<CoordType>> edge_list_;
    bool is_clockwise_{true};
};
using LinkedEdgesI = LinkedEdges<int32_t>;
using LinkedEdgesD = LinkedEdges<double>;
}  // namespace medb2
#endif  // MEDB_LINKED_EDGES_H
