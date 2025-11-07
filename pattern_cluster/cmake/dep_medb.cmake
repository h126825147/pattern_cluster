# medb library
set(pkg_name medb)

set(THIRD_PATH_DIR "${CMAKE_SOURCE_DIR}/third_party")

# include medb
set(MEDB_PATH "${THIRD_PATH_DIR}/${pkg_name}/include")

include_directories(${MEDB_PATH})

set(MEDB_LIB ${PATTERN_CLUSTER_TOP_DIR}/third_party/medb/lib/libmedb.so)
