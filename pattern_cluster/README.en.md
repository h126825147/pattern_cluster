# pattern_cluster

#### Description
{**This program is a framework of pattern cluster problems, see details in following sections**}

#### Software Architecture
The following file tree shows the file distribution of the answer frame. Participants only need to implement the PatternCluster function in the pattern_cluster.cpp file. In utils.h, several useful functions are provided. Participants can optimize these util function for higher performance.
``````
pattern_cluster
├──build/
│  ├──bin/
│  │  └──pattern_cluster                    Generated binary file
│  └──lib/
│     └──pattern_cluster.so                 Generated dynamic library for pattern_cluster_evaluator
├──cases/
│  ├──layouts/
│  │  ├──small_layout_csc.oas               Layout 1, small-scale cosine similarity constraint scenario
│  │  ├──small_layout_ecc.oas               Layout 2, small-scale edge shift constraint scenario
│  │  └──large_layout.oas                   Layout 3, large-scale layout
│  ├──case1/
│  │  └──case1_param.txt                    Matching parameters for Layout 1
│  ├──case2/
│  │  └──case2_param.txt                    Matching parameters for Layout 2
│  ├──case3/
│  │  └──case3_param.txt                    Matching parameters for cosine similarity constraint scenario in Layout 3
│  └──case4/
│     └──case4_param.txt                    Matching parameters for edge shift constraint scenario in Layout 3
├──cmake/
│  ├──dep_fftw.cmake                        CMake file for the third-party library fftw3
│  ├──dep_medb.cmake                        CMake file for HANQING Medb
│  └──util.cmake                            A set of CMake scripts for automated dependency package download and platform adaptation
├──include/
│  ├──pattern_cluster.h                     Data structures and function declarations required for pattern cluster processing(requires modifying build scripts accordingly)
│  └──utils.h                               Data structures and auxiliary function declarations provided for pattern cluster; participants can modify or add as needed (requires modifying build scripts accordingly)
├──src/
│  ├──CMakeLists.txt                        Used for compiling and generating the pattern_cluster dynamic library
│  ├──pattern_cluster.cc                    Source file to be implemented by participants; additional files can be added (requires modifying build scripts accordingly)
│  └──utils.cc                              Auxiliary functions provided for pattern cluster; participants can modify or add as needed (requires modifying build scripts accordingly)
├──third_party/                             Participants are allowed to add additional third-party libraries as needed (requires modifying build scripts accordingly)
│  ├──fftw/                                 Includes headers and library files for the third-party library fftw
│  └──medb/                                 Includes headers and library files for HANQING Medb
├──build.sh                                 Compilation script
├──CMakeLists.txt                           CMake configuration for generating the executable
├──main.cc                                  Main function source code
├──pattern_cluster_evaluator                Scoring executable
├──README.en.md                             English version of this document
├──README.md                                This document
└──run.sh                                   Runtime script
``````

#### Compilation & Execution​

1.  In the `pattern_cluster` directory​
- Run the `build.sh` script to compile the project.After running the build.sh script to complete compilation, the dynamic library libpattern_cluster.so will be generated in the `build/lib/`, and the executable file pattern_clusterwill be generated in the `build/bin/`. Participants can use pattern_cluster for lightweight operation verification.
```shell
sh build.sh
```

2.  In the `pattern_cluster` directory​
- Run the `run.sh` script to automatically perform scoring. The score file is generated in `pattern_cluster/output/`.
```shell
sh run.sh
```

#### Requirement

1. Hardware Requirements
- Common ARM platforms are acceptable, with final performance scoring based on Kunpeng-920.KunPeng-920, 64 cores per socket, 2 sockets, CPU frequency 200 ~ 2600 MHz, 1TiB System memory

2. Software Dependencies
- g++ (GCC) 10.3.0+
- cmake version 3.20.5+