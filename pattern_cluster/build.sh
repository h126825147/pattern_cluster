#!/bin/bash
# ============================================================================
# Copyright (c) 2021-2024 Huawei Technologies Co., Ltd.
# All rights reserved.
# 
# This software including any source code is protected by copyright law and international treaties.
# Huawei retains full ownership rights in the software including any source code, including, but not
# limited to, patents, copyrights, implementation and licensing rights therein, the rights to modify and use the
# software including any source code for any purpose, the right to assign or donate it to a third party.
# NO PATENT LICENSES OF ANY PARTY ARE HEREBY GRANTED BY IMPLICATION, ESTOPPEL OR OTHERWISE.
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WARRANTIES OF ANY KIND, EITHER EXPRESS OR IMPLIED, ARE DISCLAIMED,
# INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# This copyright notice must be included in all copies or derivative works.
# ============================================================================

set -e
BASEPATH=$(cd "$(dirname $0)"; pwd)
export BUILD_PATH="${BASEPATH}/build"

_COUNT=0

define_opt() {
  local var=$1
  local default=$2
  local flag=$3
  local intro=$4

  # count opt number
  _COUNT=$((${_COUNT} + 1))

  local max_flag=8
  local mod_max=$((${_COUNT} % ${max_flag}))
  local flag_space="              "
  local intro_space="     "

  # set global varible
  if [[ "X${var}" != "X" ]] && [[ "X${default}" != "X" ]]; then
      eval "${var}=${default}"
  fi
  if [[ "X${mod_max}" != "X1" ]] && [[ "X${mod_max}" != "X${_COUNT}" ]]; then
      _FLAGS="${_FLAGS}\n${flag_space}[${flag}]"
  else
      _FLAGS="${_FLAGS} [${flag}]"
  fi
  _INTROS="${_INTROS}${intro_space}${flag} ${intro}\n"
}

# define option
define_opt ""                    ""     "-h"                          "Print usage"
define_opt DEBUG_MODE            "off"  "-d"                          "Enable debug mode, default off"
define_opt VERBOSE_MODE          "off"  "-v"                          "Enable building verbose mode, defalut off"
define_opt ENABLE_ZIP            "off"  "-z"                          "Enable packing pattern_cluster , default off"
define_opt THREAD_NUM            16      "-j[n]"                       "Set the threads number when building, default 8"
define_opt ENABLE_SVE            "off"  "--sve"                       "Enable sve, default off"

# print usage message
usage()
{
  printf "Usage:\nbash build.sh${_FLAGS}\n\n"
  printf "Options:\n${_INTROS}"
}

# check and set options
checkopts()
{
  # short option follow -o, log option follow -1.
  ARGS=$(getopt -o hdvcztj:N:arA -l assert,doxygen,sve,app -n  "$0" -- "$@")
  eval set -- "$ARGS"
  
  # process the options
  while true
  do
    case "$1" in
      --sve)
        ENABLE_SVE="on"
        ;;
      -v)
        VERBOSE_MODE="on"
        ;;
      -d|--debug)
        DEBUG_MODE="on"
        ;;
      -z|--zip)
        ENABLE_ZIP="on"
        ;;
      -j)
        THREAD_NUM=$2
        shift
        ;;
      -h)
        usage
        exit 0
        ;;
      --)
        break
        ;;
      *)
        echo "Unknown option $1!"
        usage
        exit 1
    esac
    shift
  done
}

checkopts "$@"
echo "---------------- pattern_cluster: build start ----------------"

build_exit()
{
    echo "$@" >&2
    stty echo
    exit 1
}

# define path
export PATTERN_CLUSTER_BUILD_PATH="${BUILD_PATH}/"
export PATTERN_CLUSTER_BUILD_BIN_PATH="${BUILD_PATH}/bin/"
export PATTERN_CLUSTER_BUILD_LIB_PATH="${BUILD_PATH}/lib/"
export PATTERN_CLUSTER_OUTPUT_PATH="${BASEPATH}/output"

zip_pattern_cluster()
{
    if [[ "X${ENABLE_ZIP}" = "Xoff" ]]; then
      return 0
    fi
    echo "start packing evaluator."
    mkdir -p "${PATTERN_CLUSTER_OUTPUT_PATH}"
    echo "clear old version package."
    rm -rf "${PATTERN_CLUSTER_OUTPUT_PATH:?Package Path is required}"/*
    tar_path=$(which tar)
    # Get machine type, including x86/arm_920b/arm_920b
    machine_type=""
    GET_ARCH=$(arch)
    if [[ $GET_ARCH =~ "x86_64" ]];then
        machine_type="x86"
    else
        cpu_start=$(cat < /proc/cpuinfo | grep "part" | tail -n 1 | awk '{print $4}')
        if [ "$cpu_start" == "0xd01" ]; then
            machine_type="arm_920"
        elif [ "$cpu_start" == "0xd02" ]; then
            machine_type="arm_920b"
        fi
    fi
    unzip_package_name="medb"
    mkdir -p "${PATTERN_CLUSTER_OUTPUT_PATH}"/${unzip_package_name}
    rm -rf "${PATTERN_CLUSTER_OUTPUT_PATH:?}"/${unzip_package_name}/*
    cp -r "${BASEPATH}/include" "${PATTERN_CLUSTER_OUTPUT_PATH}"/${unzip_package_name}
    mkdir -p "${PATTERN_CLUSTER_OUTPUT_PATH}"/${unzip_package_name}/lib
    rm -rf "${PATTERN_CLUSTER_OUTPUT_PATH:?}"/${unzip_package_name}/lib/*
    cp "${BASEPATH}/build/hqpm/lib/libhqpm.so" "${PATTERN_CLUSTER_OUTPUT_PATH}"/${unzip_package_name}/lib
    cp -r "${BASEPATH}/third_party/medb/include/medb" "${PATTERN_CLUSTER_OUTPUT_PATH}"/${unzip_package_name}/include
    cp "${BASEPATH}/third_party/medb/lib/libmedb.so" "${PATTERN_CLUSTER_OUTPUT_PATH}"/${unzip_package_name}/lib
    cd "${PATTERN_CLUSTER_OUTPUT_PATH}"
    SHORT_COMMIT_ID=$(git rev-parse --short HEAD)
    PACKAGE_NAME="evaluator_$(date +"%Y%m%d")_${SHORT_COMMIT_ID}_${machine_type}.tar.gz"
    $tar_path -zcf ${PACKAGE_NAME} ${unzip_package_name}
    echo "finish packing pattern_cluster."
}

build_pattern_cluster()
{
    echo "start build pattern_cluster project."
    mkdir -pv ${PATTERN_CLUSTER_BUILD_PATH}
    mkdir -pv ${PATTERN_CLUSTER_BUILD_LIB_PATH}
    
    cd ${PATTERN_CLUSTER_BUILD_PATH}

    CMAKE_ARGS="-DDEBUG_MODE=$DEBUG_MODE"
    
    if [[ "X${ENABLE_SVE}" = "Xon" ]]; then
      CMAKE_ARGS="${CMAKE_ARGS} -DPATTERN_CLUSTER_ENABLE_SVE=ON"
    else
      CMAKE_ARGS="${CMAKE_ARGS} -DPATTERN_CLUSTER_ENABLE_SVE=OFF"
    fi

    if [[ "X${DEBUG_MODE}" = "Xoff" ]]; then
        CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_BUILD_TYPE=Release"
    else
        CMAKE_ARGS="${CMAKE_ARGS} -DCMAKE_BUILD_TYPE=Debug"
        CMAKE_ARGS="${CMAKE_ARGS} -DASSERT_DEBUG_MODE=ON"
    fi
    echo "${CMAKE_ARGS}"
    cmake ${CMAKE_ARGS} ${BASEPATH}
    if [[ "X${VERBOSE_MODE}" = "Xon" ]]; then
        CMAKE_VERBOSE="--verbose"
    fi

    cmake --build . ${CMAKE_VERBOSE} -j${THREAD_NUM}

    echo "success building pattern_cluster project!"
}
build_pattern_cluster
zip_pattern_cluster

echo "---------------- pattern_cluster: build end   ----------------"
