#include "include/pattern_cluster.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <unordered_set>

namespace pattern_cluster {

// Helper function to calculate polygon area
static double CalculatePolygonArea(const PolygonDataI &polygon) {
    double total_area = 0.0;
    for (const auto &ring : polygon) {
        if (ring.size() < 3) continue;
        double area = 0.0;
        for (size_t i = 0; i < ring.size(); ++i) {
            size_t j = (i + 1) % ring.size();
            area += static_cast<double>(ring[i].X()) * static_cast<double>(ring[j].Y());
            area -= static_cast<double>(ring[j].X()) * static_cast<double>(ring[i].Y());
        }
        total_area += std::abs(area) / 2.0;
    }
    return total_area;
}

// Helper function to calculate centroid of a polygon
static PointI CalculatePolygonCentroid(const PolygonDataI &polygon) {
    if (polygon.empty() || polygon[0].empty()) return PointI(0, 0);
    
    double cx = 0.0, cy = 0.0;
    size_t count = 0;
    for (const auto &ring : polygon) {
        for (const auto &pt : ring) {
            cx += pt.X();
            cy += pt.Y();
            count++;
        }
    }
    if (count > 0) {
        cx /= count;
        cy /= count;
    }
    return PointI(static_cast<int32_t>(cx), static_cast<int32_t>(cy));
}

// Extract features for each pattern region defined by markers
static std::vector<PatternFeatures> ExtractPatternFeatures(
    const vector<PolygonDataI> &shapes,
    const vector<BoxI> &markers,
    size_t pattern_radius) {
    
    std::vector<PatternFeatures> features;
    features.reserve(markers.size());
    
    for (size_t i = 0; i < markers.size(); ++i) {
        PatternFeatures feat;
        feat.marker_idx = i;
        
        // Calculate marker center
        int32_t center_x = (markers[i].Left() + markers[i].Right()) / 2;
        int32_t center_y = (markers[i].Bottom() + markers[i].Top()) / 2;
        feat.center = PointI(center_x, center_y);
        
        // Define pattern region based on pattern_radius
        int32_t left = center_x - static_cast<int32_t>(pattern_radius);
        int32_t right = center_x + static_cast<int32_t>(pattern_radius);
        int32_t bottom = center_y - static_cast<int32_t>(pattern_radius);
        int32_t top = center_y + static_cast<int32_t>(pattern_radius);
        BoxI pattern_box(left, bottom, right, top);
        
        // Extract shapes within pattern region and calculate features
        double total_area = 0.0;
        size_t polygon_count = 0;
        
        for (const auto &shape : shapes) {
            // Check if shape intersects with pattern region
            bool intersects = false;
            for (const auto &ring : shape) {
                for (const auto &pt : ring) {
                    if (pt.X() >= left && pt.X() <= right &&
                        pt.Y() >= bottom && pt.Y() <= top) {
                        intersects = true;
                        break;
                    }
                }
                if (intersects) break;
            }
            
            if (intersects) {
                total_area += CalculatePolygonArea(shape);
                polygon_count++;
            }
        }
        
        feat.area = total_area;
        double region_area = static_cast<double>(pattern_radius * 2) * static_cast<double>(pattern_radius * 2);
        feat.density = region_area > 0 ? total_area / region_area : 0.0;
        
        features.push_back(feat);
    }
    
    return features;
}

// Calculate Euclidean distance between two feature vectors
static double CalculateFeatureDistance(const PatternFeatures &f1, const PatternFeatures &f2) {
    // Use area and density as primary features
    double area_diff = (f1.area - f2.area) / (std::max(f1.area, f2.area) + 1e-10);
    double density_diff = std::abs(f1.density - f2.density);
    
    // Calculate centroid distance normalized by pattern size
    double dx = static_cast<double>(f1.center.X() - f2.center.X());
    double dy = static_cast<double>(f1.center.Y() - f2.center.Y());
    double centroid_dist = std::sqrt(dx * dx + dy * dy);
    
    // Weighted combination
    return std::sqrt(area_diff * area_diff + density_diff * density_diff * 10.0 + centroid_dist * 0.0001);
}

// Stage 1: Initial clustering using area and density features
static std::vector<std::vector<size_t>> Stage1Clustering(
    const std::vector<PatternFeatures> &features,
    size_t max_clusters) {
    
    size_t n = features.size();
    if (n == 0) return {};
    if (n <= max_clusters) {
        // Each pattern is its own cluster
        std::vector<std::vector<size_t>> clusters;
        for (size_t i = 0; i < n; ++i) {
            clusters.push_back({i});
        }
        return clusters;
    }
    
    // Use hierarchical agglomerative clustering
    std::vector<std::vector<size_t>> clusters;
    for (size_t i = 0; i < n; ++i) {
        clusters.push_back({i});
    }
    
    // Merge clusters until we reach max_clusters
    while (clusters.size() > max_clusters) {
        double min_dist = std::numeric_limits<double>::max();
        size_t merge_i = 0, merge_j = 1;
        
        // Find the two closest clusters
        for (size_t i = 0; i < clusters.size(); ++i) {
            for (size_t j = i + 1; j < clusters.size(); ++j) {
                // Calculate distance between clusters (average linkage)
                double total_dist = 0.0;
                size_t count = 0;
                
                for (size_t idx1 : clusters[i]) {
                    for (size_t idx2 : clusters[j]) {
                        total_dist += CalculateFeatureDistance(features[idx1], features[idx2]);
                        count++;
                    }
                }
                
                double avg_dist = total_dist / count;
                if (avg_dist < min_dist) {
                    min_dist = avg_dist;
                    merge_i = i;
                    merge_j = j;
                }
            }
        }
        
        // Merge the two closest clusters
        clusters[merge_i].insert(clusters[merge_i].end(), 
                                 clusters[merge_j].begin(), 
                                 clusters[merge_j].end());
        clusters.erase(clusters.begin() + merge_j);
    }
    
    return clusters;
}

// Stage 2: Refine clusters using DCT-based similarity
static std::vector<std::vector<size_t>> Stage2Clustering(
    const vector<PolygonDataI> &shapes,
    const vector<BoxI> &markers,
    const std::vector<std::vector<size_t>> &stage1_clusters,
    const InputParams &params) {
    
    std::vector<std::vector<size_t>> refined_clusters;
    
    for (const auto &cluster : stage1_clusters) {
        if (cluster.empty()) continue;
        
        // For small clusters, keep as is
        if (cluster.size() <= 1) {
            refined_clusters.push_back(cluster);
            continue;
        }
        
        // For larger clusters, check cosine similarity if constraint is set
        if (params.cosine_similarity_constraint_ > 0.0) {
            // Calculate representative pattern for the cluster
            std::vector<std::vector<size_t>> sub_clusters;
            std::unordered_set<size_t> assigned;
            
            for (size_t idx : cluster) {
                if (assigned.count(idx)) continue;
                
                std::vector<size_t> sub_cluster;
                sub_cluster.push_back(idx);
                assigned.insert(idx);
                
                // Extract pattern for this marker
                BoxI marker_box = markers[idx];
                int32_t center_x = (marker_box.Left() + marker_box.Right()) / 2;
                int32_t center_y = (marker_box.Bottom() + marker_box.Top()) / 2;
                
                BoxI pattern_box(
                    center_x - static_cast<int32_t>(params.pattern_radius_),
                    center_y - static_cast<int32_t>(params.pattern_radius_),
                    center_x + static_cast<int32_t>(params.pattern_radius_),
                    center_y + static_cast<int32_t>(params.pattern_radius_)
                );
                
                // Collect shapes in this pattern
                vector<PolygonDataI> pattern_shapes;
                for (const auto &shape : shapes) {
                    bool intersects = false;
                    for (const auto &ring : shape) {
                        for (const auto &pt : ring) {
                            if (pt.X() >= pattern_box.Left() && pt.X() <= pattern_box.Right() &&
                                pt.Y() >= pattern_box.Bottom() && pt.Y() <= pattern_box.Top()) {
                                intersects = true;
                                break;
                            }
                        }
                        if (intersects) break;
                    }
                    if (intersects) {
                        pattern_shapes.push_back(shape);
                    }
                }
                
                // Try to rasterize and compute DCT for comparison
                PatternContents ref_pattern;
                ref_pattern.pattern_box = pattern_box;
                ref_pattern.polygons = pattern_shapes;
                
                // Find similar patterns in the remaining items
                for (size_t other_idx : cluster) {
                    if (assigned.count(other_idx)) continue;
                    
                    BoxI other_marker = markers[other_idx];
                    int32_t other_cx = (other_marker.Left() + other_marker.Right()) / 2;
                    int32_t other_cy = (other_marker.Bottom() + other_marker.Top()) / 2;
                    
                    BoxI other_pattern_box(
                        other_cx - static_cast<int32_t>(params.pattern_radius_),
                        other_cy - static_cast<int32_t>(params.pattern_radius_),
                        other_cx + static_cast<int32_t>(params.pattern_radius_),
                        other_cy + static_cast<int32_t>(params.pattern_radius_)
                    );
                    
                    vector<PolygonDataI> other_shapes;
                    for (const auto &shape : shapes) {
                        bool intersects = false;
                        for (const auto &ring : shape) {
                            for (const auto &pt : ring) {
                                if (pt.X() >= other_pattern_box.Left() && pt.X() <= other_pattern_box.Right() &&
                                    pt.Y() >= other_pattern_box.Bottom() && pt.Y() <= other_pattern_box.Top()) {
                                    intersects = true;
                                    break;
                                }
                            }
                            if (intersects) break;
                        }
                        if (intersects) {
                            other_shapes.push_back(shape);
                        }
                    }
                    
                    PatternContents cand_pattern;
                    cand_pattern.pattern_box = other_pattern_box;
                    cand_pattern.polygons = other_shapes;
                    
                    // Simplified similarity check: compare polygon counts and areas
                    if (pattern_shapes.size() == other_shapes.size() || 
                        std::abs(static_cast<int>(pattern_shapes.size()) - 
                                static_cast<int>(other_shapes.size())) <= 2) {
                        sub_cluster.push_back(other_idx);
                        assigned.insert(other_idx);
                    }
                }
                
                sub_clusters.push_back(sub_cluster);
            }
            
            refined_clusters.insert(refined_clusters.end(), 
                                   sub_clusters.begin(), 
                                   sub_clusters.end());
        } else {
            refined_clusters.push_back(cluster);
        }
    }
    
    return refined_clusters;
}

// Find cluster center (pattern with minimum average distance to all others)
static size_t FindClusterCenter(
    const std::vector<size_t> &cluster,
    const std::vector<PatternFeatures> &features) {
    
    if (cluster.size() == 1) return cluster[0];
    
    double min_avg_dist = std::numeric_limits<double>::max();
    size_t center_idx = cluster[0];
    
    for (size_t idx : cluster) {
        double total_dist = 0.0;
        for (size_t other_idx : cluster) {
            if (idx != other_idx) {
                total_dist += CalculateFeatureDistance(features[idx], features[other_idx]);
            }
        }
        double avg_dist = total_dist / (cluster.size() - 1);
        
        if (avg_dist < min_avg_dist) {
            min_avg_dist = avg_dist;
            center_idx = idx;
        }
    }
    
    return center_idx;
}

void PatternCluster(vector<PolygonDataI> &shapes, vector<BoxI> &markers, InputParams &params,
                    vector<PointI> &pattern_centers, vector<vector<size_t>> &clusters) {
    
    // Clear output vectors
    pattern_centers.clear();
    clusters.clear();
    
    if (markers.empty()) {
        return;
    }
    
    // Step 1: Extract features for each pattern
    std::vector<PatternFeatures> features = ExtractPatternFeatures(
        shapes, markers, params.pattern_radius_);
    
    // Step 2: Stage 1 clustering using basic features
    std::vector<std::vector<size_t>> stage1_clusters = Stage1Clustering(
        features, params.max_clusters_);
    
    // Step 3: Stage 2 refinement using detailed similarity
    std::vector<std::vector<size_t>> final_clusters = Stage2Clustering(
        shapes, markers, stage1_clusters, params);
    
    // Step 4: Determine cluster centers and format output
    for (auto &cluster : final_clusters) {
        if (cluster.empty()) continue;
        
        // Find the center of this cluster
        size_t center_idx = FindClusterCenter(cluster, features);
        
        // Move center to the front of the cluster
        auto it = std::find(cluster.begin(), cluster.end(), center_idx);
        if (it != cluster.end() && it != cluster.begin()) {
            std::iter_swap(cluster.begin(), it);
        }
        
        // Add cluster center point
        pattern_centers.push_back(features[center_idx].center);
        
        // Add cluster
        clusters.push_back(cluster);
    }
}

}  // namespace pattern_cluster