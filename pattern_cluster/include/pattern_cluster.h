#ifndef INCLUDE_PATTERN_CLUSTER_H_
#define INCLUDE_PATTERN_CLUSTER_H_

#include "utils.h"
#include <cstddef>
#include <vector>
#include <cmath>


namespace pattern_cluster {
using namespace medb2;

/**
 * @brief Structure to hold extracted features for a pattern
 */
struct PatternFeatures {
    size_t marker_idx;          // Index of the marker
    PointI center;              // Center point of the pattern
    double area;                // Total area of polygons
    double density;             // Polygon density in the pattern region
    std::vector<double> dct_features; // DCT features for pattern matching
};
/**
 * @brief Defines the input parameter structure for the `PatternCluster` function, 
 * containing various parameters required for pattern clustering.
 */
struct InputParams {
        /**
     * @brief The radius of the pattern, used to determine the effective range of the pattern.
     */
    size_t pattern_radius_ = 0;
    /**
     * @brief The maximum number of clusters to be formed during the clustering process.
     */
    size_t max_clusters_ = 0;
    /**
     * @brief The area constraint parameter, used to limit the area of the clusters.
     */
    double cosine_similarity_constraint_ = 0.0;
    /**
     * @brief The edge move constraint parameter, used to limit the movement of edges during clustering.
     */
    size_t edge_move_constraint_ = 0;
};

void PatternCluster(vector<PolygonDataI> &shapes, vector<BoxI> &markers,
    InputParams &params, vector<PointI> &pattern_centers, vector<vector<size_t>> &clusters);

}  // namespace pattern_cluster

#endif  // INCLUDE_PATTERN_CLUSTER_H_