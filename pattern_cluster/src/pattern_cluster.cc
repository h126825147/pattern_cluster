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
        
        // Use utility function to get shapes within the pattern region
        // Note: Since we don't have a Layout pointer here, we'll use manual intersection
        vector<PolygonDataI> pattern_shapes;
        
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
                pattern_shapes.push_back(shape);
            }
        }
        
        // Calculate total area and density
        double total_area = 0.0;
        for (const auto &shape : pattern_shapes) {
            total_area += CalculatePolygonArea(shape);
        }
        
        feat.area = total_area;
        double region_area = static_cast<double>(pattern_radius * 2) * static_cast<double>(pattern_radius * 2);
        feat.density = region_area > 0 ? total_area / region_area : 0.0;
        
        // Generate DCT features if there are shapes in the pattern
        if (!pattern_shapes.empty() && pattern_radius > 0) {
            try {
                PatternContents pattern_content;
                pattern_content.pattern_box = pattern_box;
                pattern_content.polygons = pattern_shapes;
                
                // Rasterize the pattern (use size 32 for efficiency)
                size_t raster_size = 32;
                auto raster_matrix = Rasterize(pattern_content, raster_size);
                
                // Flatten and apply DCT
                auto flattened = Flatten(raster_matrix);
                auto dct_result = FFTWDCT(flattened, raster_size, raster_size);
                
                // Keep only low-frequency components for feature representation
                size_t feature_count = std::min(size_t(64), dct_result.size());
                feat.dct_features.assign(dct_result.begin(), dct_result.begin() + feature_count);
            } catch (...) {
                // If rasterization fails, leave dct_features empty
                feat.dct_features.clear();
            }
        }
        
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
    
    // If DCT features are available, use them for more accurate comparison
    double dct_dist = 0.0;
    if (!f1.dct_features.empty() && !f2.dct_features.empty()) {
        // Use cosine similarity between DCT features
        double cos_sim = CosSimilarity(f1.dct_features, f2.dct_features);
        // Convert similarity to distance (1 - similarity)
        dct_dist = 1.0 - cos_sim;
    }
    
    // Weighted combination (adjust weights based on feature availability)
    if (!f1.dct_features.empty() && !f2.dct_features.empty()) {
        return std::sqrt(area_diff * area_diff * 0.5 + density_diff * density_diff * 5.0 + 
                        centroid_dist * 0.00001 + dct_dist * dct_dist * 10.0);
    } else {
        return std::sqrt(area_diff * area_diff + density_diff * density_diff * 10.0 + 
                        centroid_dist * 0.0001);
    }
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
    const std::vector<PatternFeatures> &features,
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
            // Split cluster based on cosine similarity threshold
            std::vector<std::vector<size_t>> sub_clusters;
            std::unordered_set<size_t> assigned;
            
            for (size_t idx : cluster) {
                if (assigned.count(idx)) continue;
                
                std::vector<size_t> sub_cluster;
                sub_cluster.push_back(idx);
                assigned.insert(idx);
                
                const auto &ref_feat = features[idx];
                
                // Find similar patterns based on DCT features
                for (size_t other_idx : cluster) {
                    if (assigned.count(other_idx)) continue;
                    
                    const auto &cand_feat = features[other_idx];
                    
                    // Check cosine similarity if DCT features are available
                    if (!ref_feat.dct_features.empty() && !cand_feat.dct_features.empty()) {
                        double cos_sim = CosSimilarity(ref_feat.dct_features, cand_feat.dct_features);
                        
                        // If similarity exceeds threshold, add to this sub-cluster
                        if (cos_sim >= params.cosine_similarity_constraint_) {
                            sub_cluster.push_back(other_idx);
                            assigned.insert(other_idx);
                        }
                    } else {
                        // Fallback to area and density similarity
                        double area_ratio = std::min(ref_feat.area, cand_feat.area) / 
                                          (std::max(ref_feat.area, cand_feat.area) + 1e-10);
                        double density_diff = std::abs(ref_feat.density - cand_feat.density);
                        
                        if (area_ratio > 0.8 && density_diff < 0.1) {
                            sub_cluster.push_back(other_idx);
                            assigned.insert(other_idx);
                        }
                    }
                }
                
                sub_clusters.push_back(sub_cluster);
            }
            
            refined_clusters.insert(refined_clusters.end(), 
                                   sub_clusters.begin(), 
                                   sub_clusters.end());
        } else {
            // No cosine similarity constraint, keep the cluster as is
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
        features, stage1_clusters, params);
    
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