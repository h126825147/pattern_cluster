#include "utils.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <fftw3.h>

#include "medb/point_utils.h"
#include "medb/element_iterator.h"
#include "medb/gdsii.h"
#include "medb/goa.h"
#include "medb/oasis.h"
#include "medb/hgs.h"
namespace pattern_cluster {
using namespace std;
using namespace medb2;

Layout *ReadFile(string input_path) {
    Layout *layout_ptr;
    if (input_path.substr(input_path.length() - 3, 3) == "gds") {
        layout_ptr = gdsii::Read(input_path);
    } else if (input_path.substr(input_path.length() - 3, 3) == "oas") {
        layout_ptr = oasis::Read(input_path);
    } else if (input_path.substr(input_path.length() - 3, 3) == "hgs") {
        layout_ptr = hgs::Read(input_path);
    }else {
        cout << "The input file format is not supported, Exiting the program!" << endl;
        terminate();
    }
    return layout_ptr;
}

std::vector<medb2::PolygonPtrDataI> TransPolysPtr(std::vector<medb2::PolygonDataI> &polys) {
    std::vector<medb2::PolygonPtrDataI> result;
    result.reserve(polys.size());
    std::transform(polys.begin(), polys.end(), std::back_inserter(result), [](medb2::PolygonDataI &rings) {
        medb2::PolygonPtrDataI ring_ptrs;
        ring_ptrs.reserve(rings.size());
        std::transform(rings.begin(), rings.end(), std::back_inserter(ring_ptrs),
                       [](RingDataI &ring) { return &ring; });
        return ring_ptrs;
    });
    return result;
}

vector<PolygonDataI> GetShapes(const Layout *layout, const Layer shape_layer, const BoxI &window) {
    Cell *cell = layout->TopCell();
    ElementIteratorOption option(*cell, shape_layer);
    option.SetMaxLevel(numeric_limits<int>::max());
    option.SetType(QueryElementType::kOnlyShape);
    option.SetNeedPolygonData(true);
    option.SetRegion(window);
    ElementIterator ele_iter(option);
    vector<PolygonDataI> shapes;
    for (ele_iter.Begin(); !ele_iter.IsEnd(); ele_iter.Next()) {
        auto shape_data = ele_iter.CurrentPolygonData();
        shapes.emplace_back(move(shape_data));
    }
    PolygonDataI domain = {{PointI(window.Left(), window.Bottom()), PointI(window.Left(), window.Top()),
                            PointI(window.Right(), window.Top()), PointI(window.Right(), window.Bottom())}};
    vector<medb2::PolygonDataI> include_domain = {domain};
    vector<PolygonDataI> result_shapes;
    medb2::goa::BooleanManhattan(medb2::BooleanType::kAnd, TransPolysPtr(shapes), TransPolysPtr(include_domain),
                                 medb2::ManhattanCompressType::kNoCompress, result_shapes);
    shapes = result_shapes;
    return shapes;
}

RingDataI ScaleBoxRing(const BoxI &box, size_t new_width, size_t new_height) {
    PointI central_point((box.Left() + box.Right()) / 2, (box.Bottom() + box.Top()) / 2);
    int left = central_point.X() - new_width / 2;
    int right = central_point.X() + new_width / 2;
    int bottom = central_point.Y() - new_height / 2;
    int top = central_point.Y() + new_height / 2;
    PointI p1(left, bottom);
    PointI p2(left, top);
    PointI p3(right, top);
    PointI p4(right, bottom);
    return {p1, p2, p3, p4};
}

RingDataI ShiftRing(const RingDataI &ring, int h_shift, int v_shift) {
    RingDataI shifted_ring;
    for (const auto &point : ring) {
        PointI shifted_point(point.X() + h_shift, point.Y() + v_shift);
        shifted_ring.emplace_back(shifted_point);
    }
    return shifted_ring;
}

PolygonDataI ShiftPolygon(const PolygonDataI &polygon, int h_shift, int v_shift) {
    PolygonDataI shifted_polygon;
    for (const auto &ring : polygon) {
        RingDataI shifted_ring = ShiftRing(ring, h_shift, v_shift);
        shifted_polygon.emplace_back(shifted_ring);
    }
    return shifted_polygon;
}

vector<PolygonDataI> ShiftPolygons(const vector<PolygonDataI> &polygons, int h_shift, int v_shift) {
    vector<PolygonDataI> shifted_polygons;
    for (const auto &polygon : polygons) {
        PolygonDataI shifted_polygon = ShiftPolygon(polygon, h_shift, v_shift);
        shifted_polygons.emplace_back(shifted_polygon);
    }
    return shifted_polygons;
}

vector<PointI> GetSeeds(Layout *layout, Layer seed_layer, BoxI &window) {
    Cell *cell = layout->TopCell();
    ElementIteratorOption option(*cell, seed_layer);
    option.SetMaxLevel(numeric_limits<int>::max());
    option.SetType(QueryElementType::kOnlyShape);
    option.SetNeedPolygonData(true);
    option.SetRegion(window);
    ElementIterator ele_iter(option);
    vector<PointI> seeds;
    for (ele_iter.Begin(); !ele_iter.IsEnd(); ele_iter.Next()) {
        auto shape_data = ele_iter.CurrentPolygonData();
        RingDataI hull = shape_data[0];
        int32_t max_x = max(hull[0].X(), max(hull[1].X(), max(hull[2].X(), hull[3].X())));
        int32_t min_x = min(hull[0].X(), min(hull[1].X(), min(hull[2].X(), hull[3].X())));
        int32_t max_y = max(hull[0].Y(), max(hull[1].Y(), max(hull[2].Y(), hull[3].Y())));
        int32_t min_y = min(hull[0].Y(), min(hull[1].Y(), min(hull[2].Y(), hull[3].Y())));
        int32_t x = (max_x + min_x) / 2;
        int32_t y = (max_y + min_y) / 2;
        seeds.emplace_back(x, y);
    }
    return seeds;
}

void RemoveLayers(Layout *layout_ptr, const vector<Layer> &layers) {
    Cell *cell = layout_ptr->TopCell();
    for (auto &layer : layers) {
        cell->Remove(layer);
    }
}

void AddShapesIntoLayout(const Layout *layout_ptr, const Layer &shape_layer, const vector<PolygonDataI> &new_shapes) {
    Cell *cell = layout_ptr->TopCell();
    for (auto &poly : new_shapes) {
        for (auto &ring : poly) {
            PolygonI poly_ring(ring);
            cell->InsertShape(shape_layer, poly_ring);
        }
    }
}

void WriteIntoFile(Layout *layout_ptr, string output_path) {
    if (output_path.substr(output_path.length() - 3, 3) == "gds") {
        gdsii::Write(layout_ptr, output_path);
    } else if (output_path.substr(output_path.length() - 3, 3) == "oas") {
        oasis::Write(layout_ptr, output_path);
    }
    else if (output_path.substr(output_path.length() - 3, 3) == "hgs") {
        hgs::Write(layout_ptr, output_path);
    }

}

PointI PointAdd(const PointI &a, const PointI &b) { return PointI(a.X() + b.X(), a.Y() + b.Y()); }

PointI PointSub(const PointI &a, const PointI &b) { return PointI(a.X() - b.X(), a.Y() - b.Y()); }

vector<PolygonDataI> ClipPattern(vector<PolygonDataI> &polys, const BoxI &area) {
    vector<PolygonDataI> out_group;
    auto polys_ptr = TransPolysPtr(polys);
    goa::Intersect(polys_ptr, area, out_group);
    return out_group;
}

struct VerticalEdge {
    size_t x;
    size_t y_min;
    size_t y_max;
    
    VerticalEdge(size_t x, size_t y_min, size_t y_max) : x(x), y_min(y_min), y_max(y_max) {}
};

void FillPolygonsInMatrix(const PolygonDataI& polygons, vector<vector<int>>& matrix, size_t rows, size_t cols) {
    vector<vector<VerticalEdge>> edge_table(rows);

    for (const auto& poly : polygons) {
        if (poly.size() < 4) continue;
            
        vector<pair<size_t, size_t>> image_poly;
        for (const auto& pt : poly) {
            image_poly.push_back({static_cast<size_t>(pt.X()), static_cast<size_t>(pt.Y())});
        }
        
        for (size_t i = 0; i < image_poly.size(); ++i) {
            const pair<size_t, size_t>& p1 = image_poly[i];
            const pair<size_t, size_t>& p2 = image_poly[(i + 1) % image_poly.size()];

            if (p1.first == p2.first) { // vertical_edges
                size_t y_min = min(p1.second, p2.second);
                size_t y_max = max(p1.second, p2.second);
                
                if (y_min < rows) {
                    size_t y_index = min(y_min, rows - 1);
                    edge_table[y_index].emplace_back(p1.first, y_min, y_max);
                }
            }
        }
    }

    vector<VerticalEdge> active_edges; 
    for (size_t y = 0; y < rows; ++y) {    // Scanline processing of vertical edges
        if (y < rows) {
            for (const auto& edge : edge_table[y]) {
                active_edges.push_back(edge);
            }
        }

        active_edges.erase(     // Remove the ending edge(y_max <= y)
            remove_if(active_edges.begin(), active_edges.end(),
                [y](const VerticalEdge& e) { return e.y_max <= y; }),
            active_edges.end()
        );
        
        sort(active_edges.begin(), active_edges.end(),      // Sort active edges by x-coordinate
            [](const VerticalEdge& a, const VerticalEdge& b) {
                return a.x < b.x;
            }
        );
        
        for (size_t i = 0; i < active_edges.size(); i += 2) {   // Odd counting within the polygon
            if (i + 1 >= active_edges.size()) break;
            
            size_t x_start = active_edges[i].x;
            size_t x_end = min(cols - 1, active_edges[i + 1].x);
            
            for (size_t x = x_start; x < x_end; ++x) {
                if (x < cols) {
                    matrix[y][x] = 1;
                }
            }
        }
    }
}

vector<vector<double>> CompressBinaryMatrix(const vector<vector<int>>& src, size_t target_size) {
    const double scale = static_cast<double>(src.size()) / static_cast<double>(target_size);
    vector<vector<double>> comp_matrix(target_size, vector<double>(target_size, 0.0));
    vector<vector<double>> weight_sum(target_size, vector<double>(target_size, 0.0));

    vector<double> grid_bounds(target_size + 1);
    for (size_t i = 0; i <= target_size; ++i) {
        grid_bounds[i] = i * scale;  // Calculate all boundaries
    }

    for (size_t src_y = 0; src_y < src.size(); ++src_y) {
        const double pixel_top = src_y;
        const double pixel_bottom = src_y + 1.0;
        size_t start_y = max(size_t(0), static_cast<size_t>(floor(pixel_top / scale)));
        size_t end_y = min(target_size, static_cast<size_t>(floor(pixel_bottom / scale)) + 1);
        
        for (size_t src_x = 0; src_x < src.size(); ++src_x) {
            const double pixel_value = src[src_y][src_x];
            if (pixel_value == 0) {
                continue;
            }            
            const double pixel_left = src_x;
            const double pixel_right = src_x + 1.0;

            size_t start_x = max(size_t(0), static_cast<size_t>(floor(pixel_left / scale)));
            size_t end_x = min(target_size, static_cast<size_t>(floor(pixel_right / scale)) + 1);
            
            for (size_t comp_y = start_y; comp_y < end_y; ++comp_y) {  // Traverse the overlapping area
                const double grid_top = grid_bounds[comp_y];
                const double grid_bottom = grid_bounds[comp_y + 1];
                
                for (size_t comp_x = start_x; comp_x < end_x; ++comp_x) {
                    const double grid_left = grid_bounds[comp_x];
                    const double grid_right = grid_bounds[comp_x + 1];

                    const double overlap_left = max(pixel_left, grid_left);
                    const double overlap_right = min(pixel_right, grid_right);
                    const double overlap_top = max(pixel_top, grid_top);
                    const double overlap_bottom = min(pixel_bottom, grid_bottom);
                    
                    const double overlap_width = max(0.0, overlap_right - overlap_left);
                    const double overlap_height = max(0.0, overlap_bottom - overlap_top);
                    const double overlap_area = overlap_width * overlap_height;
                    
                    if (overlap_area > 0) {
                        comp_matrix[comp_y][comp_x] += pixel_value * overlap_area;
                        weight_sum[comp_y][comp_x] += overlap_area;
                    }
                }
            }
        }
    }
  
    for (size_t y = 0; y < target_size; ++y) {      //  normalization
        for (size_t x = 0; x < target_size; ++x) {
            if (weight_sum[y][x] > 0) {
                comp_matrix[y][x] /= scale * scale;
            }
        }
    }
    return comp_matrix;
}

vector<vector<double>> Rasterize(const PatternContents &Pattern, size_t target_size) {   // target_row is the number of rows in the target matrix.
    size_t height = Pattern.pattern_box.Height();
    size_t width = Pattern.pattern_box.Width();
    vector<vector<int>> matrix(height, vector<int>(width, 0));
    for (auto &polygon : Pattern.polygons) {
        PolygonDataI temp_polygon;
        for (auto &ring : polygon) {
            RingDataI temp_ring;
            for (auto &point : ring) {
                temp_ring.emplace_back(point.X() - Pattern.pattern_box.Left(), point.Y() - Pattern.pattern_box.Bottom());
            }
            temp_polygon.emplace_back(move(temp_ring));
        }
        FillPolygonsInMatrix(temp_polygon, matrix, width, height);
    }
    return CompressBinaryMatrix(matrix, target_size);
}

vector<double> Flatten(const vector<vector<double>>& matrix) {
    vector<double> result;
    for (const auto& row : matrix) {
        result.insert(result.end(), row.begin(), row.end());
    }
    return result;
}

vector<double> FFTWDCT(vector<double> &input, size_t rows, size_t cols) {
    vector<double> output(rows * cols, 0.0);
    auto plan = fftw_plan_r2r_2d(rows, cols, input.data(), output.data(), FFTW_REDFT10, FFTW_REDFT10, FFTW_ESTIMATE);
    fftw_execute_r2r(plan, input.data(), output.data());
    fftw_destroy_plan(plan);
    return output;
}

double CosSimilarity(const vector<double> &ref, const vector<double> &candidate) {
    if (ref.size() != candidate.size()) {
        cout << "Inconsistent feature vector dimensions." << endl;
    }
    double dot_product = 0.0;
    double norm_ref = 0.0;
    double norm_candidate = 0.0;
    for (size_t i = 0; i < ref.size(); ++i) {
        dot_product += ref[i] * candidate[i];
        norm_ref += ref[i] * ref[i];
        norm_candidate += candidate[i] * candidate[i];
    }
    if (norm_ref == 0 || norm_candidate == 0) {
        return 0.0;
    }
    return dot_product / (sqrt(norm_ref) * sqrt(norm_candidate));
}
}  // namespace pattern_cluster
