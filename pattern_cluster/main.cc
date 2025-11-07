#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "include/pattern_cluster.h"
#include "include/utils.h"
#include "medb/allocator.h"
#include "medb/element_iterator.h"
#include "medb/goa.h"
#include "medb/layout.h"

using namespace std;
using namespace medb2;

namespace pattern_cluster {

constexpr size_t PAGE_SIZE = 1;
enum class MemInfoType {
    RSS,  // Resident Set Size
    HWM   // High Water Mark
};

uint64_t GetThreadMemValue(MemInfoType type) {
    std::ifstream ifs("/proc/self/status");
    if (!ifs) {
        std::cerr << "Profiling error: cannot open system proc status file.\n";
        return false;  // Use max value to indicate failure
    }

    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss(line);
        std::string tag;
        iss >> tag;
        if ((type == MemInfoType::HWM && tag == "VmHWM:") || (type == MemInfoType::RSS && tag == "VmRSS:")) {
            uint64_t value;
            iss >> value;
            return value;
        }
    }
    return static_cast<uint64_t>(-1);
}

void TouchMemory(void *mem, size_t size) {
    for (size_t i = 0; i < size; i += PAGE_SIZE) {
        volatile char *ptr = reinterpret_cast<char *>(mem) + i;
        *ptr = 1;
    }
}

void FixMemDiff(uint64_t &rss_mem, uint64_t &hwm_mem) {
    int64_t mem_diff_kb = hwm_mem - rss_mem;
    if (mem_diff_kb > 0) {
        // RSS 小于 HWM，需要分配内存
        uint64_t mem_diff_bytes = mem_diff_kb * 1024 + 1024 * 1024;
        void *extra_memory = malloc(mem_diff_bytes);
        if (extra_memory != nullptr) {
            TouchMemory(extra_memory, mem_diff_bytes);
            sleep(1);
            // 再次检查 RSS 以确保分配生效
            uint64_t new_rss_kb = GetThreadMemValue(MemInfoType::RSS);
            std::cout << "Extra memory allocated. New RSS: " << new_rss_kb << " kb" << std::endl;
        } else {
            std::cerr << "Memory allocation failed." << std::endl;
        }
    } else {
        std::cout << "No extra memory needed. RSS is equal to or exceeds HWM." << std::endl;
    }
}

void ResetPeakMemoryUsage() {
    int pid = getpid();
    std::string file_path = "/proc/" + std::to_string(pid) + "/clear_refs";
    std::ofstream clear_refs(file_path, std::ios::out);

    uint64_t rss_kb = GetThreadMemValue(MemInfoType::RSS);
    uint64_t hwm_kb = GetThreadMemValue(MemInfoType::HWM);
    FixMemDiff(rss_kb, hwm_kb);
}

inline vector<PolygonDataI> GetPolygonDatas(const Layout *layout, const Layer &layer) {
    medb2::ElementIteratorOption option(*(layout->TopCell()), layer);
    option.SetType(medb2::QueryElementType::kOnlyShape);
    option.SetNeedPolygonData(true);
    medb2::ElementIterator iter(option);
    vector<PolygonDataI> polygons;
    for (iter.Begin(); !iter.IsEnd(); iter.Next()) {
        auto poly_data = iter.CurrentPolygonData();
        polygons.emplace_back(poly_data);
    }
    return polygons;
}

inline vector<medb2::BoxI> GetPatternMakers(const Layout *layout, const Layer &layer) {
    auto top_cell = layout->TopCell();
    Shapes shapes = top_cell->GetShapes(layer);
    shapes.Decompress();
    auto &boxes = shapes.Raw<BoxI>();
    vector<medb2::BoxI> pattern_boxes;
    pattern_boxes.reserve(boxes.size());
    for (auto &box : boxes) {
        pattern_boxes.emplace_back(box);
    }
    return pattern_boxes;
}

InputParams GetInputParams(const string &param_file) {
    ifstream file(param_file);
    if (!file.is_open()) {
        cerr << "Error: Fail to open the param file: " << param_file << endl;
    }
    InputParams input_params;
    file >> input_params.pattern_radius_ >> input_params.max_clusters_ >> input_params.cosine_similarity_constraint_ >>
        input_params.edge_move_constraint_;
    return input_params;
}

void WriteClusters(const std::string &clusters_file, const std::vector<std::vector<size_t>> &clusters) {
    std::ofstream file(clusters_file);
    if (!file.is_open()) {
        std::cerr << "Error: Fail to open the clusters file: " << clusters_file << std::endl;
        return;
    }
    file << clusters.size() << endl;
    for (const auto &cluster : clusters) {
        if (cluster.empty()) {
            std::cerr << "Error: Cluster size cannot be zero." << clusters_file << std::endl;
            return;
        }
        for (size_t i = 0; i < cluster.size() - 1; ++i) {
            file << cluster[i] << ",";
        }
        file << cluster.back() << endl;
    }
    file.close();
}

void WritePatternCenters(const string &pattern_centers_file, const vector<PointI> &pattern_centers) {
    ofstream file(pattern_centers_file);
    if (!file.is_open()) {
        cerr << "Error: Fail to open the pattern centers file: " << pattern_centers_file << endl;
        return;
    }
    for (auto &center : pattern_centers) {
        file << center.X() << "," << center.Y() << endl;
    }
    file.close();
}

struct ClusterProfileInfo {
    size_t hwm_mem_ = 0;
    std::chrono::nanoseconds duration_ = std::chrono::nanoseconds(0);
};

ClusterProfileInfo ClusterProfile(const string &layout_file, const string &param_file, const string &centers_file,
                                  const string &clusters_file) {
    std::cout << "Info: Start Profile Cluster Function" << std::endl;
    medb_malloc_trim();
    ResetPeakMemoryUsage();
    auto const hwm_mem_start = GetThreadMemValue(MemInfoType::HWM);
    auto start = std::chrono::steady_clock::now();
    medb2::Layout *layout = ReadFile(layout_file);
    auto polys = GetPolygonDatas(layout, Layer(1, 0));
    auto makers = GetPatternMakers(layout, Layer(2, 0));
    auto params = GetInputParams(param_file);
    vector<PointI> pattern_centers;
    vector<vector<size_t>> clusters;
    pattern_cluster::PatternCluster(polys, makers, params, pattern_centers, clusters);
    WritePatternCenters(centers_file, pattern_centers);
    WriteClusters(clusters_file, clusters);
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    auto const hwm_mem_end = GetThreadMemValue(MemInfoType::HWM);
    ClusterProfileInfo profile_info;
    profile_info.hwm_mem_ = hwm_mem_end - hwm_mem_start;
    profile_info.duration_ = duration;
    std::cout << "Info: pattern cluster time taken: " << std::fixed << std::setprecision(6)
              << chrono::duration_cast<std::chrono::duration<double, std::milli>>(duration).count() << " (ms)"
              << std::endl;
    std::cout << "Info: pattern cluster hwm mem: " << std::fixed << std::setprecision(6)
              << static_cast<double>(profile_info.hwm_mem_) / 1024 << " (MB)" << std::endl;
    return profile_info;
}
}  // namespace pattern_cluster

void Usage(char **argv) {
    cout << "This program is a frame shows how to solve Pattern Cluster Problems with medb parser. " << endl;
    cout << "Usage: " << filesystem::path(argv[0]).filename().string() << " [options]" << endl;
    cout << "Options:" << endl;
    cout << "  -layout          <layout_file>           Read and process the gds or oas or hgs file." << endl;
    cout << "  -param           <param_file>            Read and process the gds or oas or hgs file." << endl;
    cout << "  -pattern_centers <pattern_centers_file>  The first output path of pattern centers which should within "
            "each markers."
         << endl;
    cout << "  -clusters        <clusters_file>         The second output path of cluster info.\n"
         << "                                           The first line is the number of clusters.\n"
         << "                                           From the second line to the end, each line represents a "
            "cluster,\n"
         << "                                           which contians all marker ids in this cluster.\n"
         << "                                           The first marker id in one line is the cluster center." << endl;
    cout << "  -h               Display this help message" << endl;
}

bool CheckFilePathEnd(const string &str, const string &suffix) {
    if (str.length() < suffix.length()) {
        return false;
    }
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        cerr << "Error: No options provided." << endl;
        Usage(argv);
        return 1;
    }
    if (string(argv[1]) == "-h") {
        Usage(argv);
        return 0;
    }
    string layout_file = "";
    string param_file = "";
    string centers_file = "";
    string clusters_file = "";
    for (int i = 1; i < argc - 1; i = i + 2) {
        if (string(argv[i]) == "-layout") {
            layout_file = argv[i + 1];
        } else if (string(argv[i]) == "-param") {
            param_file = argv[i + 1];
        } else if (string(argv[i]) == "-pattern_centers") {
            centers_file = argv[i + 1];
        } else if (string(argv[i]) == "-clusters") {
            clusters_file = argv[i + 1];
        } else {
            cerr << "Error: Unknown option '" << argv[i] << "'." << endl;
            Usage(argv);
            return 1;
        }
    }
    if (!CheckFilePathEnd(layout_file, ".oas") && !CheckFilePathEnd(layout_file, ".gds")&& !CheckFilePathEnd(layout_file, ".hgs")) {
        cerr << "Error: Layout path should be .gds or .oas or .hgs format." << endl;
    }
    if (!CheckFilePathEnd(param_file, ".txt")) {
        cerr << "Error: Parameter file path should be .txt." << endl;
    }
    if (!CheckFilePathEnd(centers_file, ".txt")) {
        cerr << "Error: Pattern center file path should be .txt." << endl;
    }
    if (!CheckFilePathEnd(clusters_file, ".txt")) {
        cerr << "Error: Cluster file path should be .txt." << endl;
    }
    cout << "Info: Layout file path: " << layout_file << endl;
    cout << "Info: Parameter file path: " << param_file << endl;
    cout << "Info: Pattern center file path: " << centers_file << endl;
    cout << "Info: Cluster file path: " << clusters_file << endl;
    pattern_cluster::ClusterProfile(layout_file, param_file, centers_file, clusters_file);
    return 0;
}