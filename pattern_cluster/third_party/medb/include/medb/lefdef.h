/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024. All rights reserved.
 */

#ifndef MEDB_LEFDEF_H
#define MEDB_LEFDEF_H

#include <string>
#include <vector>
#include "medb/base_utils.h"

namespace medb2 {
class Layout;
namespace lef_def {

/**
 * @brief 加载指定的 lef 或 def 文件，支持同时加载多个 lef 文件，也支持仅加载 1 个 def 文件
 *
 * @param lef_file 待加载的 lef 文件集合
 * @param def_file 待加载的 def 文件
 * @return 成功则返回 Layout 指针对象(由调用者管理)，失败则返回 nullptr
 */
Layout MEDB_API *Read(const std::vector<std::string> &lef_file, const std::string &def_file);

}  // namespace lef_def
}  // namespace medb2

#endif  // MEDB_LEFDEF_H
