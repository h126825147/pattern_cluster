cmake_minimum_required(VERSION 3.14)
project(pattern_cluster VERSION 0.1.0)

add_definitions(-DVERSION_SUFFIX=\"alpha\")

# path
set(PATTERN_CLUSTER_TOP_DIR ${CMAKE_CURRENT_LIST_DIR})
set(PATTERN_CLUSTER_PROJECT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Set the output path of executable files.
set(EXECUTABLE_OUTPUT_PATH ${PATTERN_CLUSTER_PROJECT_DIR}/build/bin)
set(LIBRARY_OUTPUT_PATH ${PATTERN_CLUSTER_PROJECT_DIR}/build/lib)

# Set the module name perfix
set(MODULE_NAME_PERFIX pattern_cluster)

# cxx flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wformat -Wall -Werror -Wno-error=maybe-uninitialized -Wextra -Wtrampolines -Wdate-time -Wformat=2 -Wno-variadic-macros -Wno-error=deprecated-copy -fopenmp")

# enable the -O3 option only in release mode
if(CMAKE_BUILD_TYPE MATCHES "Release")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

# enable the -rdynamic option only in debug mode
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -rdynamic")
endif()

set(link_opts
    -Wl,--whole-archive
    -lpthread
    -lstdc++fs
    -ldl
    -lz
    -Wl,--no-whole-archive
)

# Include dependencies of third party
include(cmake/utils.cmake)
include(cmake/dep_medb.cmake)
include(cmake/dep_fftw.cmake)

# include
include_directories(${PATTERN_CLUSTER_TOP_DIR})
include_directories(${PATTERN_CLUSTER_TOP_DIR}/include)

add_subdirectory(src)

# generate executable files
add_executable(pattern_cluster_e2e main.cc)

# bind a dynamic link to an executable file
target_link_libraries(pattern_cluster_e2e PRIVATE
    pattern_cluster_lib
    ${link_opts}
    ${MEDB_LIB}
    ${FFTW_LIB}
)

set_target_properties(pattern_cluster_e2e PROPERTIES OUTPUT_NAME "pattern_cluster")