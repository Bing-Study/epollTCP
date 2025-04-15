cmake_minimum_required(VERSION 3.10)
project(epoll_microgrid C)

# 设置C标准
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# 包含目录配置
include_directories(
        ${PROJECT_SOURCE_DIR}/include
)

# 递归收集所有源文件
file(GLOB_RECURSE SOURCES
        "${PROJECT_SOURCE_DIR}/src/*.c"
)

# 生成可执行文件
add_executable(${PROJECT_NAME}
        ${SOURCES}
        include/thread_pool.h
)

# 链接系统库
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME}
        PRIVATE
        Threads::Threads
)

# 安装规则
install(TARGETS ${PROJECT_NAME}
        DESTINATION bin
)
install(DIRECTORY include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
)

# 构建类型配置
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-g -O0 -Wall -Wextra)
else()
    add_compile_options(-O2)
    add_definitions(-DNDEBUG)
endif()

# 测试支持（需要时启用）
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    enable_testing()
    add_subdirectory(tests)
endif()

# 可选：静态分析配置
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY clang-tidy)
    if(CLANG_TIDY)
        set(CMAKE_C_CLANG_TIDY ${CLANG_TIDY})
    endif()
endif()
