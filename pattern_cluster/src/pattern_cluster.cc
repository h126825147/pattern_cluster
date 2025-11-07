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
double CalculatePatternSimilarity(const PatternContents& p1, const PatternContents& p2, size_t raster_size = 64) {
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
bool CheckEdgeMovementConstraint(const PatternContents& p1, const PatternContents& p2, size_t constraint) {
    // 简化实现：检查图形数量和边界框差异
    // 图形数量较少的pattern的每个图形必须与另一个pattern中的仅一个图形存在重叠
    
    const auto& polys1 = p1.polygons;
    const auto& polys2 = p2.polygons;
    
    // 如果其中一个为空，不满足约束
    if (polys1.empty() || polys2.empty()) {
        return false;
    }
    
    // 检查边界框的偏移是否在约束范围内
    int dx = abs(p1.pattern_box.Left() - p2.pattern_box.Left());
    int dy = abs(p1.pattern_box.Bottom() - p2.pattern_box.Bottom());
    
    // 如果两个pattern的边界框偏移过大，则不满足约束
    if (dx > static_cast<int>(constraint) || dy > static_cast<int>(constraint)) {
        return false;
    }
    
    // 使用图形数量和面积作为启发式判断
    // 如果两个pattern的图形数量和面积接近，则可能满足约束
    size_t count_diff = (polys1.size() > polys2.size()) ? 
                        (polys1.size() - polys2.size()) : (polys2.size() - polys1.size());
    
    // 如果图形数量差异过大，不满足约束
    if (count_diff > polys1.size() / 2 && count_diff > polys2.size() / 2) {
        return false;
    }
    
    return true;
}

// 贪心聚类算法：基于余弦相似度
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
            
            double similarity = CalculatePatternSimilarity(patterns[i], patterns[j]);
            if (similarity >= threshold) {
                cluster.push_back(j);
                clustered[j] = true;
            }
        }
        
        clusters.push_back(cluster);
    }
    
    // 处理未聚类的pattern（如果超过max_clusters限制）
    if (clusters.size() >= max_clusters) {
        // 将剩余未聚类的pattern分配到最相似的聚类中
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
            
            if (CheckEdgeMovementConstraint(patterns[i], patterns[j], constraint)) {
                cluster.push_back(j);
                clustered[j] = true;
            }
        }
        
        clusters.push_back(cluster);
    }
    
    // 处理未聚类的pattern
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
                // 如果没有满足约束的聚类，分配到最后一个聚类
                if (!assigned && !clusters.empty()) {
                    clusters.back().push_back(i);
                }
            }
        }
    }
}

// 从marker中选择最优的pattern中心点
PointI SelectOptimalCenter(const vector<PolygonDataI>& shapes, const BoxI& marker, size_t radius) {
    // 策略：在marker范围内采样多个候选中心点，选择包含最多图形面积的点
    PointI marker_center((marker.Left() + marker.Right()) / 2, 
                         (marker.Bottom() + marker.Top()) / 2);
    
    // 如果marker很小，直接返回中心
    if (marker.Width() <= 10 || marker.Height() <= 10) {
        return marker_center;
    }
    
    // 采样策略：在marker内部采样多个候选点
    vector<PointI> candidates;
    candidates.push_back(marker_center);
    
    // 添加四个角附近的点（向中心偏移一些）
    int offset = std::min(static_cast<int>(marker.Width() / 4), static_cast<int>(marker.Height() / 4));
    candidates.push_back(PointI(marker.Left() + offset, marker.Bottom() + offset));
    candidates.push_back(PointI(marker.Right() - offset, marker.Bottom() + offset));
    candidates.push_back(PointI(marker.Left() + offset, marker.Top() - offset));
    candidates.push_back(PointI(marker.Right() - offset, marker.Top() - offset));
    
    // 添加边的中点
    candidates.push_back(PointI(marker_center.X(), marker.Bottom() + offset));
    candidates.push_back(PointI(marker_center.X(), marker.Top() - offset));
    candidates.push_back(PointI(marker.Left() + offset, marker_center.Y()));
    candidates.push_back(PointI(marker.Right() - offset, marker_center.Y()));
    
    // 评估每个候选点：选择包含最多图形的点
    PointI best_center = marker_center;
    size_t max_polygon_count = 0;
    
    for (const auto& candidate : candidates) {
        // 确保候选点在marker内
        if (candidate.X() < marker.Left() || candidate.X() > marker.Right() ||
            candidate.Y() < marker.Bottom() || candidate.Y() > marker.Top()) {
            continue;
        }
        
        // 提取pattern并计算图形数量
        PatternContents pattern = ExtractPattern(shapes, candidate, radius);
        if (pattern.polygons.size() > max_polygon_count) {
            max_polygon_count = pattern.polygons.size();
            best_center = candidate;
        }
    }
    
    return best_center;
}

void PatternCluster(vector<PolygonDataI> &shapes, vector<BoxI> &markers, InputParams &params,
                    vector<PointI> &pattern_centers, vector<vector<size_t>> &clusters) {
    // 清空输出
    pattern_centers.clear();
    clusters.clear();
    
    // 如果没有marker，直接返回
    if (markers.empty()) {
        return;
    }
    
    // 第一步：为每个marker选择最优的pattern中心点
    pattern_centers.reserve(markers.size());
    for (const auto& marker : markers) {
        PointI center = SelectOptimalCenter(shapes, marker, params.pattern_radius_);
        pattern_centers.push_back(center);
    }
    
    // 第二步：提取每个pattern的内容
    vector<PatternContents> patterns;
    patterns.reserve(pattern_centers.size());
    for (const auto& center : pattern_centers) {
        patterns.push_back(ExtractPattern(shapes, center, params.pattern_radius_));
    }
    
    // 第三步：根据约束类型进行聚类
    // 判断使用哪种约束：余弦相似度 vs 边偏移
    if (params.cosine_similarity_constraint_ > 0) {
        // 使用余弦相似度约束进行聚类
        ClusterBySimilarity(patterns, params.cosine_similarity_constraint_, 
                           params.max_clusters_, clusters);
    } else if (params.edge_move_constraint_ > 0) {
        // 使用边偏移约束进行聚类
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
    
    // 第四步：确保每个聚类的第一个元素是聚类中心（代表性pattern）
    // 选择聚类中最接近聚类几何中心的pattern作为聚类中心
    for (auto& cluster : clusters) {
        if (cluster.size() <= 1) continue;
        
        // 计算聚类中所有pattern中心点的几何中心
        int64_t sum_x = 0, sum_y = 0;
        for (size_t idx : cluster) {
            sum_x += pattern_centers[idx].X();
            sum_y += pattern_centers[idx].Y();
        }
        PointI centroid(sum_x / cluster.size(), sum_y / cluster.size());
        
        // 找到距离几何中心最近的pattern
        size_t best_idx = 0;
        int64_t min_dist = std::numeric_limits<int64_t>::max();
        for (size_t i = 0; i < cluster.size(); ++i) {
            const PointI& p = pattern_centers[cluster[i]];
            int64_t dx = p.X() - centroid.X();
            int64_t dy = p.Y() - centroid.Y();
            int64_t dist = dx * dx + dy * dy;
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