/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2022. All rights reserved.
 */

#ifndef MEDB_ERRCODE_H
#define MEDB_ERRCODE_H

#include <cstdint>
namespace medb2 {

using MEDB_RESULT = uintptr_t;

constexpr MEDB_RESULT MEDB_SUCCESS = 0;
constexpr MEDB_RESULT MEDB_FAILURE = -1;

constexpr MEDB_RESULT MEDB_ERR_BASE = 0x1000;

constexpr MEDB_RESULT MEDB_PARAM_ERR = MEDB_ERR_BASE + 0x1;
constexpr MEDB_RESULT MEDB_LAYER_EXIST = MEDB_ERR_BASE + 0x2;
constexpr MEDB_RESULT MEDB_FILE_FORMAT_ERR = MEDB_ERR_BASE + 0x3;
constexpr MEDB_RESULT MEDB_LAYER_NOEXIST = MEDB_ERR_BASE + 0x5;
constexpr MEDB_RESULT MEDB_TOP_CELL_NULL = MEDB_ERR_BASE + 0x7;

}  // namespace medb2

#endif  // MEDB_ERRCODE_H
