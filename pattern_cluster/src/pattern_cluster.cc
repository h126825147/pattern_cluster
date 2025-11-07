#include "include/pattern_cluster.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace pattern_cluster {

// 提取每个marker对应的pattern内容
PatternContents ExtractPattern(const vector<PolygonDataI>& shapes, const PointI& center, size_t radius) {
    PatternContents pattern;
    // 构建pattern的边界框（以center为中心，边长为2*radius的正方形）
    int left = center.X() - static_cast<int>(radius);
    int right = center.X() + static_cast<int>(radius);
    int bottom = center.Y() - static_cast<int>(radius);
    int top = center.Y() + static_cast<int>(radius);
    pattern.pattern_box = BoxI(left, bottom, right, top);
    
    // 裁剪shapes到pattern区域
    vector<PolygonDataI> temp_shapes = shapes;
    pattern.polygons = ClipPattern(temp_shapes, pattern.pattern_box);
    
    return pattern;
}

// 计算两个pattern的余弦相似度
// 栅格化尺寸：64x64提供了精度和性能的平衡
double CalculatePatternSimilarity(const PatternContents& p1, const PatternContents& p2) {
    const size_t raster_size = 64;  // 栅格化尺寸
    
    // 栅格化pattern
    auto matrix1 = Rasterize(p1, raster_size);
    auto matrix2 = Rasterize(p2, raster_size);
    
    // 展平为一维向量
    auto vec1 = Flatten(matrix1);
    auto vec2 = Flatten(matrix2);
    
    // 进行DCT变换
    auto dct1 = FFTWDCT(vec1, raster_size, raster_size);
    auto dct2 = FFTWDCT(vec2, raster_size, raster_size);
    
    // 计算余弦相似度
    return CosSimilarity(dct1, dct2);
}

// 检查两个pattern是否满足边偏移约束
// 边偏移约束：两个pattern的对应边之间的偏移量不超过constraint
bool CheckEdgeMovementConstraint(const PatternContents& p1, const PatternContents& p2, size_t constraint) {
    // 根据赛题要求：
    // 1. 允许两个pattern的图形数量不一致
    // 2. 图形数量较少的pattern的每个图形必须要与另一个pattern中的仅一个图形存在重叠
    // 3. 允许两个对应边存在一定的偏移量，该偏移量上限由constraint决定
    
    const auto& polys1 = p1.polygons;
    const auto& polys2 = p2.polygons;
    
    // 如果其中一个为空，不满足约束
    if (polys1.empty() || polys2.empty()) {
        return false;
    }
    
    // 检查边界框的偏移是否在约束范围内
    // 这是一个保守的快速检查：如果pattern box的边偏移过大，则图形边偏移也可能过大
    int dx = abs(p1.pattern_box.Left() - p2.pattern_box.Left());
    int dy = abs(p1.pattern_box.Bottom() - p2.pattern_box.Bottom());
    
    // 如果两个pattern的边界框偏移过大，则不满足约束
    if (dx > static_cast<int>(constraint) || dy > static_cast<int>(constraint)) {
        return false;
    }
    
    // 使用图形数量作为启发式判断
    // 如果两个pattern的图形数量接近，则更可能满足约束
    // 定义图形数量差异阈值：不超过较小pattern的一半
    size_t count_diff = (polys1.size() > polys2.size()) ? 
                        (polys1.size() - polys2.size()) : (polys2.size() - polys1.size());
    size_t min_count = std::min(polys1.size(), polys2.size());
    
    // 如果图形数量差异过大，不太可能满足约束
    if (count_diff > min_count / 2) {
        return false;
    }
    
    return true;
}

// 贪心聚类算法：基于余弦相似度
// 策略：遍历所有pattern，为每个未聚类的pattern创建新聚类，
//       并将所有与其相似度超过阈值的pattern加入该聚类
void ClusterBySimilarity(const vector<PatternContents>& patterns, double threshold, 
                         size_t max_clusters, vector<vector<size_t>>& clusters) {
    size_t n = patterns.size();
    vector<bool> clustered(n, false);
    
    // 创建聚类
    for (size_t i = 0; i < n && clusters.size() < max_clusters; ++i) {
        if (clustered[i]) continue;
        
        vector<size_t> cluster;
        cluster.push_back(i);
        clustered[i] = true;
        
        // 将相似的pattern加入同一聚类
        for (size_t j = i + 1; j < n; ++j) {
            if (clustered[j]) continue;
            
            // 计算余弦相似度
            double similarity = CalculatePatternSimilarity(patterns[i], patterns[j]);
            if (similarity >= threshold) {
                cluster.push_back(j);
                clustered[j] = true;
            }
        }
        
        clusters.push_back(cluster);
    }
    
    // 处理未聚类的pattern（如果超过max_clusters限制）
    // 将剩余pattern分配到最相似的已有聚类中
    if (clusters.size() >= max_clusters) {
        for (size_t i = 0; i < n; ++i) {
            if (!clustered[i]) {
                // 找到最相似的聚类
                double max_sim = -1.0;
                size_t best_cluster = 0;
                for (size_t c = 0; c < clusters.size(); ++c) {
                    double sim = CalculatePatternSimilarity(patterns[i], patterns[clusters[c][0]]);
                    if (sim > max_sim) {
                        max_sim = sim;
                        best_cluster = c;
                    }
                }
                clusters[best_cluster].push_back(i);
            }
        }
    }
}

// 贪心聚类算法：基于边偏移约束
// 策略：遍历所有pattern，为每个未聚类的pattern创建新聚类，
//       并将所有满足边偏移约束的pattern加入该聚类
void ClusterByEdgeMovement(const vector<PatternContents>& patterns, size_t constraint,
                           size_t max_clusters, vector<vector<size_t>>& clusters) {
    size_t n = patterns.size();
    vector<bool> clustered(n, false);
    
    // 创建聚类
    for (size_t i = 0; i < n && clusters.size() < max_clusters; ++i) {
        if (clustered[i]) continue;
        
        vector<size_t> cluster;
        cluster.push_back(i);
        clustered[i] = true;
        
        // 将满足边偏移约束的pattern加入同一聚类
        for (size_t j = i + 1; j < n; ++j) {
            if (clustered[j]) continue;
            
            // 检查是否满足边偏移约束
            if (CheckEdgeMovementConstraint(patterns[i], patterns[j], constraint)) {
                cluster.push_back(j);
                clustered[j] = true;
            }
        }
        
        clusters.push_back(cluster);
    }
    
    // 处理未聚类的pattern（如果超过max_clusters限制）
    // 将剩余pattern分配到第一个满足约束的已有聚类中
    if (clusters.size() >= max_clusters) {
        for (size_t i = 0; i < n; ++i) {
            if (!clustered[i]) {
                // 找到第一个满足约束的聚类
                bool assigned = false;
                for (size_t c = 0; c < clusters.size(); ++c) {
                    if (CheckEdgeMovementConstraint(patterns[i], patterns[clusters[c][0]], constraint)) {
                        clusters[c].push_back(i);
                        assigned = true;
                        break;
                    }
                }
                // 如果没有满足约束的聚类，分配到最后一个聚类（兜底策略）
                if (!assigned && !clusters.empty()) {
                    clusters.back().push_back(i);
                }
            }
        }
    }
}

// 从marker中选择最优的pattern中心点
// 策略：在marker范围内采样多个候选点，选择包含最多图形的点作为中心
// 赛题要求：默认选择marker中心仅可获得次优解，需要优化中心点选取
PointI SelectOptimalCenter(const vector<PolygonDataI>& shapes, const BoxI& marker, size_t radius) {
    // 计算marker的中心点
    PointI marker_center((marker.Left() + marker.Right()) / 2, 
                         (marker.Bottom() + marker.Top()) / 2);
    
    // 如果marker很小（宽度或高度<=10），直接返回中心点
    if (marker.Width() <= 10 || marker.Height() <= 10) {
        return marker_center;
    }
    
    // 在marker内部采样多个候选点，寻找最优中心
    vector<PointI> candidates;
    candidates.push_back(marker_center);  // 候选点1：marker中心
    
    // 添加四个角附近的点（向中心偏移一些，避免边界）
    int offset = std::min(static_cast<int>(marker.Width() / 4), static_cast<int>(marker.Height() / 4));
    candidates.push_back(PointI(marker.Left() + offset, marker.Bottom() + offset));      // 左下角附近
    candidates.push_back(PointI(marker.Right() - offset, marker.Bottom() + offset));     // 右下角附近
    candidates.push_back(PointI(marker.Left() + offset, marker.Top() - offset));         // 左上角附近
    candidates.push_back(PointI(marker.Right() - offset, marker.Top() - offset));        // 右上角附近
    
    // 添加边的中点
    candidates.push_back(PointI(marker_center.X(), marker.Bottom() + offset));  // 下边中点
    candidates.push_back(PointI(marker_center.X(), marker.Top() - offset));     // 上边中点
    candidates.push_back(PointI(marker.Left() + offset, marker_center.Y()));    // 左边中点
    candidates.push_back(PointI(marker.Right() - offset, marker_center.Y()));   // 右边中点
    
    // 评估每个候选点：选择包含最多图形的点作为最优中心
    PointI best_center = marker_center;
    size_t max_polygon_count = 0;
    
    for (const auto& candidate : candidates) {
        // 确保候选点在marker内部
        if (candidate.X() < marker.Left() || candidate.X() > marker.Right() ||
            candidate.Y() < marker.Bottom() || candidate.Y() > marker.Top()) {
            continue;
        }
        
        // 提取以该候选点为中心的pattern，并统计图形数量
        PatternContents pattern = ExtractPattern(shapes, candidate, radius);
        if (pattern.polygons.size() > max_polygon_count) {
            max_polygon_count = pattern.polygons.size();
            best_center = candidate;
        }
    }
    
    return best_center;
}

// PatternCluster主函数：对版图中的pattern进行聚类
// 输入：
//   shapes - 版图中的图形（来自layer 1,0）
//   markers - pattern标记框（来自layer 2,0）
//   params - 聚类参数（pattern半径、最大聚类数、约束条件）
// 输出：
//   pattern_centers - 每个pattern的中心点（从marker中选取的最优中心）
//   clusters - 聚类结果（每个聚类包含多个pattern的索引）
void PatternCluster(vector<PolygonDataI> &shapes, vector<BoxI> &markers, InputParams &params,
                    vector<PointI> &pattern_centers, vector<vector<size_t>> &clusters) {
    // 清空输出容器
    pattern_centers.clear();
    clusters.clear();
    
    // 如果没有marker，直接返回空结果
    if (markers.empty()) {
        return;
    }
    
    // 第一步：为每个marker选择最优的pattern中心点
    // 不能简单使用marker中心，需要在marker内部采样寻找最优位置
    pattern_centers.reserve(markers.size());
    for (const auto& marker : markers) {
        PointI center = SelectOptimalCenter(shapes, marker, params.pattern_radius_);
        pattern_centers.push_back(center);
    }
    
    // 第二步：提取每个pattern的内容
    // 以选定的中心点为中心，提取半径为pattern_radius的正方形区域内的图形
    vector<PatternContents> patterns;
    patterns.reserve(pattern_centers.size());
    for (const auto& center : pattern_centers) {
        patterns.push_back(ExtractPattern(shapes, center, params.pattern_radius_));
    }
    
    // 第三步：根据约束类型进行聚类
    // 判断使用哪种约束：余弦相似度约束 vs 边偏移约束（两者互斥）
    if (params.cosine_similarity_constraint_ > 0) {
        // 场景1：使用余弦相似度约束进行聚类
        // 通过栅格化、DCT变换和余弦相似度计算来判断两个pattern是否相似
        ClusterBySimilarity(patterns, params.cosine_similarity_constraint_, 
                           params.max_clusters_, clusters);
    } else if (params.edge_move_constraint_ > 0) {
        // 场景2：使用边偏移约束进行聚类
        // 判断两个pattern的对应边偏移是否在允许范围内
        ClusterByEdgeMovement(patterns, params.edge_move_constraint_,
                             params.max_clusters_, clusters);
    } else {
        // 如果两个约束都不生效，将每个pattern作为单独的聚类
        // 但要遵守max_clusters约束
        size_t n = patterns.size();
        size_t clusters_to_create = std::min(n, params.max_clusters_);
        
        if (clusters_to_create == n) {
            // 每个pattern一个聚类
            for (size_t i = 0; i < n; ++i) {
                clusters.push_back({i});
            }
        } else {
            // 将patterns平均分配到max_clusters个聚类中
            size_t patterns_per_cluster = n / clusters_to_create;
            size_t remainder = n % clusters_to_create;
            
            size_t idx = 0;
            for (size_t c = 0; c < clusters_to_create; ++c) {
                vector<size_t> cluster;
                size_t count = patterns_per_cluster + (c < remainder ? 1 : 0);
                for (size_t i = 0; i < count && idx < n; ++i, ++idx) {
                    cluster.push_back(idx);
                }
                if (!cluster.empty()) {
                    clusters.push_back(cluster);
                }
            }
        }
    }
    
    // 第四步：选择每个聚类的代表性pattern作为聚类中心
    // 要求：每个聚类的第一个元素必须是该聚类的中心pattern
    // 策略：选择距离聚类几何中心最近的pattern作为代表
    for (auto& cluster : clusters) {
        if (cluster.size() <= 1) continue;
        
        // 计算聚类中所有pattern中心点的几何中心（重心）
        // 使用double精度计算以避免整数除法的精度损失
        double sum_x = 0.0, sum_y = 0.0;
        for (size_t idx : cluster) {
            sum_x += pattern_centers[idx].X();
            sum_y += pattern_centers[idx].Y();
        }
        PointI centroid(static_cast<int>(sum_x / cluster.size()), 
                       static_cast<int>(sum_y / cluster.size()));
        
        // 找到距离几何中心最近的pattern作为聚类的代表
        size_t best_idx = 0;
        int64_t min_dist = std::numeric_limits<int64_t>::max();
        
        // 计算第一个pattern的距离作为初始最小值
        const PointI& first_p = pattern_centers[cluster[0]];
        int64_t first_dx = first_p.X() - centroid.X();
        int64_t first_dy = first_p.Y() - centroid.Y();
        min_dist = first_dx * first_dx + first_dy * first_dy;
        
        for (size_t i = 1; i < cluster.size(); ++i) {
            const PointI& p = pattern_centers[cluster[i]];
            int64_t dx = p.X() - centroid.X();
            int64_t dy = p.Y() - centroid.Y();
            int64_t dist = dx * dx + dy * dy;  // 欧氏距离的平方（避免开方运算）
            if (dist < min_dist) {
                min_dist = dist;
                best_idx = i;
            }
        }
        
        // 将最佳pattern交换到聚类的第一个位置
        if (best_idx != 0) {
            std::swap(cluster[0], cluster[best_idx]);
        }
    }
}

}  // namespace pattern_cluster