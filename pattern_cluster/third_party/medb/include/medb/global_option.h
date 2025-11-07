/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */
#ifndef MEDB_GLOBAL_OPTION_H
#define MEDB_GLOBAL_OPTION_H

#include <cstdint>
#include "medb/base_utils.h"

namespace medb2 {
// kOptKeySize标识OptionKey有效值个数，如需增加枚举，请加在kOptKeySize前一个位置，不能给具体值
// kOptMinLogLevel : 设置范围为[0, 4]：0:debug, 1:info, 2: warning, 3:error, 4:fatal
enum class OptionKey : uint16_t { kOptMinLogLevel = 0, kOptKeySize };

bool MEDB_API GetOption(OptionKey opt_key, int64_t &opt_val);
bool MEDB_API SetOption(OptionKey opt_key, int64_t opt_val);
}  // namespace medb2

#endif
