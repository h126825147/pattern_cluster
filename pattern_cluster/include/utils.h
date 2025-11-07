#ifndef INCLUDE_UTILS_H_
#define INCLUDE_UTILS_H_

#include <string>
#include "medb/gdsii.h"
#include "medb/layer.h"
#include "medb/layout.h"
#include "medb/oasis.h"
#include "medb/polygon.h"

namespace pattern_cluster {
using namespace std;
using namespace medb2;
struct PatternContents {
    BoxI pattern_box;
    vector<PolygonDataI> polygons;
};
/// @brief Obtain the layout pointer by gds or oas file path.
/// @param input_path Input gds or oas file path.
/// @return Layout pointer.
Layout *ReadFile(string input_path);

/// @brief Transform PolygonDataI vector to PolygonPtrDataI vector.
/// @param polys The input PolygonDataI vector.
/// @return The output PolygonPtrDataI vector.
std::vector<medb2::PolygonPtrDataI> TransPolysPtr(std::vector<medb2::PolygonDataI> &polys);

/// @brief Get Shapes from a specific layer of top-cell of a layout pointer within a window region.
/// @param layout The Layout pointer.
/// @param shape_layer The Layer.
/// @param window The Region that all shapes should locate.
/// @return All shapes in the layer within the window.
vector<PolygonDataI> GetShapes(const Layout *layout, const Layer shape_layer, const BoxI &window);

/// @brief Resize a Box with its origin center and new width&height.
/// @param box The origin box.
/// @param new_width The new width.
/// @param new_height The new height.
/// @return The ring(vector<PointI>) of the new resized box.
RingDataI ScaleBoxRing(const BoxI &box, size_t new_width, size_t new_height);

/// @brief Shift the ring with horizontal and vertical shift distance.
/// @param ring The origin ring.
/// @param h_shift The horizontal shift distance.
/// @param v_shift The vertical shift distance.
/// @return The shifted ring.
RingDataI ShiftRing(const RingDataI &ring, int h_shift, int v_shift);

/// @brief Shift the polygon with horizontal and vertical shift distance.
/// @param ring The origin polygon.
/// @param h_shift The horizontal shift distance.
/// @param v_shift The vertical shift distance.
/// @return The shifted polygon.
PolygonDataI ShiftPolygon(const PolygonDataI &ring, int h_shift, int v_shift);

/// @brief Shift the polygon vector with horizontal and vertical shift distance.
/// @param ring The origin polygon vector.
/// @param h_shift The horizontal shift distance.
/// @param v_shift The vertical shift distance.
/// @return The shifted polygon vector.
vector<PolygonDataI> ShiftPolygons(const vector<PolygonDataI> &polygons, int h_shift, int v_shift);

/// @brief Get Seed points from a specific layer of top-cell of a layout pointer within a window region.
/// @param layout The Layout pointer.
/// @param seed_layer The layer.
/// @param window The window contains all seeds.
/// @return Seed points.
vector<PointI> GetSeeds(Layout *layout, Layer seed_layer, BoxI &window);

/// @brief Remove several layers from a layout pointer.
/// @param layout_ptr Layout Pointer.
/// @param layers The vector of removing target layers.
void RemoveLayers(Layout *layout_ptr, const vector<Layer> &layers);

/// @brief Add shapes into a specific layer of top-cell of a layout pointer.
/// @param layout_ptr The Layout pointer.
/// @param shape_layer The target layer.
/// @param new_shapes The new shapes.
void AddShapesIntoLayout(const Layout *layout_ptr, const Layer &shape_layer, const vector<PolygonDataI> &new_shapes);

/// @brief Write contents from a layout pointer into a specific gds or oas file path
///        (if the path exists, the origin contents will be cleaned).
/// @param layout_ptr Layout Pointer.
/// @param output_path Output gds or oas file path
void WriteIntoFile(Layout *layout_ptr, string output_path);

/// @brief Add two points.
/// @param a Point a.
/// @param b Point b.
/// @return The sum of a and b.
PointI PointAdd(const PointI &a, const PointI &b);

/// @brief Subtract two points.
/// @param a Point a.
/// @param b Point b.
/// @return The difference of a and b.
PointI PointSub(const PointI &a, const PointI &b);

/// @brief Clip the polygons within the area.
/// @param polys The input polygons.
/// @param area The clipping area.
/// @return The clipped polygons.
vector<PolygonDataI> ClipPattern(vector<PolygonDataI> &polys, const BoxI &area);

/// @brief Rasterize the layout.
/// @param Pattern The input Pattern.
/// @param target_size The target_size is the number of rows in the target matrix.
/// @return The matrix after rasterization.
vector<vector<double>> Rasterize(const PatternContents &Pattern, size_t target_size);

/// @brief Flatten a two-dimensional vector (matrix) into a one-dimensional vector.
/// @param matrix The input matrix.
/// @return The flattened vector.
vector<double> Flatten(const vector<vector<double>>& matrix);

/// @brief The Discrete Cosine Transform provided by FFTW3.
/// @param input The input matrix.
/// @param rows The matrix row.
/// @param cols The matrix col.
/// @return The matrix after DCT.
vector<double> FFTWDCT(vector<double> &input, size_t rows, size_t cols);

/// @brief Cosine similarity calculation.
/// @param ref Ref matrix.
/// @param candidate Candidate matrix.
/// @return Cosine similarity of the two matrices.
double CosSimilarity(const vector<double> &ref, const vector<double> &candidate);
}  // namespace pattern_cluster
#endif  // INCLUDE_UTILS_H_
