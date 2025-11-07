# pattern_cluster

#### 介绍
{**此程序是Pattern cluster的答题框架，请参阅以下部分中的详细信息**}

#### 软件架构
以下文件树展示了答题框架的文件分布，参与者只需在 pattern_cluster.cpp 文件中实现 PatternCluster 函数。在 utils.h 中，提供了几个有用的函数,参与者可以优化这些实用函数以提高性能。

``````
pattern_cluster
├──build/
│  ├──bin/
│  │  └──pattern_cluster                    生成的二进制文件
│  └──lib/
│     └──pattern_cluster.so                 生成的动态库，用于pattern_cluster_evaluator
├──cases/
│  ├──layouts/
│  │  ├──small_layout_csc.oas               版图1，小规模余弦相似度限制场景
│  │  ├──small_layout_ecc.oas               版图2，小规模边偏移限制场景
│  │  └──large_layout.oas                   版图3，大规模版图
│  ├──case1/
│  │  └──case1_param.txt                    版图1匹配参数
│  ├──case2/
│  │  └──case2_param.txt                    版图2匹配参数
│  ├──case3/
│  │  └──case3_param.txt                    版图3余弦相似度限制场景匹配参数
│  └──case4/
│     └──case4_param.txt                    版图3边偏移限制场景匹配参数
├──cmake/
│  ├──dep_fftw.cmake                        三方库fftw3的CMake文件
│  ├──dep_medb.cmake                        汉擎底座的CMake文件
│  └──util.cmake                            一套用于​​自动化依赖包下载与平台适配的工具集CMake脚本
├──include/
│  ├──pattern_cluster.h                     pattern cluster 过程中需要使用的数据结构及函数声明
│  └──utils.h                               pattern cluster 赛题方提供的数据结构及辅助函数声明，可自行添加修改(此时需自己修改构建脚本)
├──src/
│  ├──CMakeLists.txt                        用于pattern_cluster动态库的生成编译
│  ├──pattern_cluster.cc                    待参赛者实现的源文件，可自行添加其他文件(此时需自己修改构建脚本)
│  └──utils.cc                              pattern cluster 赛题方提供的辅助函数，可自行添加修改(此时需自己修改构建脚本)
├──third_party/                             允许参赛者自行添加所需要的额外三方库(此时需自己修改构建脚本)
│  ├──fftw/                                 包含三方库fftw的头文件和库文件
│  └──medb/                                 包含汉擎底座的头文件和库文件
├──build.sh                                 编译脚本
├──CMakeLists.txt                           用于pattern_cluster可执行文件的生成编译
├──main.cc                                  主函数源代码
├──pattern_cluster_evaluator                判分可执行文件
├──README.en.md                             本文档的英语版本
├──README.md                                本文档
└──run.sh                                   运行脚本
``````

#### 编译与运行

1.  在`pattern_cluster`目录下
- 运行build.sh脚本完成编译。运行该脚本会在`build/lib/`目录下生成动态库 libpattern_cluster.so ，并在`build/bin/`目录下生成可执行文件 pattern_cluster 。参赛者可以通过 pattern_cluster 进行轻量级运行验证。
```shell
sh build.sh
```

2.  在`pattern_cluster`目录下
- 运行run.sh脚本，自动进行判分，得分文件生成在`pattern_cluster/output/`下
```shell
sh run.sh
```

#### 环境依赖

1. 硬件要求
- 常见ARM平台均可，最终性能评分以KunPeng-920为准。KunPeng-920, 64 cores per socket, 2 sockets, CPU 200 ~ 2600 MHz, 1TiB System memory

2. 软件依赖
- g++ (GCC) 10.3.0+
- cmake version 3.20.5+