# fftw library

set(FFTW_PATH "${PATTERN_CLUSTER_TOP_DIR}/third_party/fftw/include")

include_directories(${FFTW_PATH})

set(FFTW_LIB ${PATTERN_CLUSTER_TOP_DIR}/third_party/fftw/lib/libfftw3.so)