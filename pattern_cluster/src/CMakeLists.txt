# Collect all source files
file(GLOB_RECURSE LIB_SRCS LIST_DIRECTORIES false *.cc)

# Create dynamic library targets
add_library(pattern_cluster_lib SHARED ${LIB_SRCS})

# Set the library output properties
set_target_properties(pattern_cluster_lib PROPERTIES
    OUTPUT_NAME "pattern_cluster"
    ARCHIVE_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
    LIBRARY_OUTPUT_DIRECTORY ${LIBRARY_OUTPUT_PATH}
)

# include
target_include_directories(pattern_cluster_lib PRIVATE
    ${PATTERN_CLUSTER_TOP_DIR}/include
)

# Link dependencies
target_link_libraries(pattern_cluster_lib PRIVATE
    ${MEDB_LIB}
    ${FFTW_LIB}
    pthread
    stdc++fs
)

if(PATTERN_CLUSTER_ENABLE_SVE AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "aarch64")
    target_compile_options(pattern_cluster_lib PRIVATE -march=armv8.5-a+sve)
endif()