cmake_minimum_required(VERSION 3.16)

project(fractal_terrain
    VERSION 1.0.0
    DESCRIPTION "基于 OpenGL 的分形地形生成与三维漫游演示系统"
    LANGUAGES CXX C)

# ---------------------------------------------------------------
# 全局编译选项
# ---------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 默认 Release（单配置生成器如 Ninja/Makefile 才生效；VS 多配置生成器需用 --config 指定）
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 把可执行文件统一输出到 bin/，方便 shaders 目录相对定位
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY        ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG  ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# ---------------------------------------------------------------
# 依赖获取策略
#   ON  : 自动从 Gitee 镜像下载 GLFW / GLM（需要联网 + Git）
#   OFF : 使用本地已下载的库（需自行设置 GLFW_ROOT / GLM_ROOT）
#
# 说明：GLAD 不再作为依赖。本项目改用自写的 src/GLLoader.{h,cpp}
#       在运行时向显卡驱动查询 OpenGL 3.3 函数地址，功能等价，
#       但无需下载任何第三方加载器源码。
# ---------------------------------------------------------------
option(USE_FETCHCONTENT "自动从 Gitee 镜像下载第三方依赖" ON)

# 依赖源码统一放到 build/_deps 外的独立目录，便于反复构建复用
set(FT_DEPS_DIR "${CMAKE_SOURCE_DIR}/third_party" CACHE PATH "第三方依赖源码目录")

if(USE_FETCHCONTENT)
    include(FetchContent)
    set(USE_FETCHCONTENT_MSG "Gitee 镜像自动下载")
    set(FETCHCONTENT_BASE_DIR "${CMAKE_BINARY_DIR}/_deps")
else()
    set(USE_FETCHCONTENT_MSG "本地路径 (GLFW_ROOT/GLM_ROOT)")
endif()

if(USE_FETCHCONTENT)
    # ---- GLFW（Gitee 镜像）----
    set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

    # 优先用本地已克隆的 third_party/glfw，避免每次重新下载
    if(EXISTS "${FT_DEPS_DIR}/glfw/CMakeLists.txt")
        message(STATUS "使用本地 GLFW: ${FT_DEPS_DIR}/glfw")
        FetchContent_Declare(glfw
            SOURCE_DIR "${FT_DEPS_DIR}/glfw")
    else()
        FetchContent_Declare(glfw
            GIT_REPOSITORY https://gitee.com/mirrors/glfw.git
            GIT_TAG        3.3.9
            GIT_SHALLOW    TRUE)
    endif()
    FetchContent_MakeAvailable(glfw)

    # ---- GLM（Gitee 镜像）----
    set(GLM_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
    set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)

    if(EXISTS "${FT_DEPS_DIR}/glm/CMakeLists.txt")
        message(STATUS "使用本地 GLM: ${FT_DEPS_DIR}/glm")
        FetchContent_Declare(glm
            SOURCE_DIR "${FT_DEPS_DIR}/glm")
    else()
        FetchContent_Declare(glm
            GIT_REPOSITORY https://gitee.com/mirrors/glm.git
            GIT_TAG        0.9.9.8
            GIT_SHALLOW    TRUE)
    endif()
    FetchContent_MakeAvailable(glm)
else()
    # ---- 本地依赖路径（USE_FETCHCONTENT=OFF 时使用）----
    set(GLFW_ROOT "" CACHE PATH "GLFW 根目录（含 include/ 与 lib/）")
    set(GLM_ROOT  "" CACHE PATH "GLM 根目录")

    if(NOT GLFW_ROOT OR NOT GLM_ROOT)
        message(FATAL_ERROR
            "USE_FETCHCONTENT=OFF 时必须设置 GLFW_ROOT / GLM_ROOT")
    endif()

    find_package(glfw3 REQUIRED HINTS ${GLFW_ROOT})
    find_package(glm   REQUIRED HINTS ${GLM_ROOT})
endif()

# ---------------------------------------------------------------
# 源码
# ---------------------------------------------------------------
set(FT_HEADERS
    src/GLLoader.h
    src/Noise.h
    src/DiamondSquare.h
    src/Terrain.h
    src/TerrainField.h
    src/Camera.h
    src/Shader.h)

set(FT_SOURCES
    src/GLLoader.cpp
    src/main.cpp)

add_executable(fractal_terrain ${FT_SOURCES} ${FT_HEADERS})

# ---------------------------------------------------------------
# 头文件搜索路径
# ---------------------------------------------------------------
target_include_directories(fractal_terrain PRIVATE src)

# ---- GLM 头文件路径 ----
# GLM 是 header-only 库。当 GLM_BUILD_LIBRARY=OFF 时它不会导出带
# INTERFACE_INCLUDE_DIRECTORIES 的 target，因此这里显式补上头文件目录。
if(TARGET glm::glm)
    target_link_libraries(fractal_terrain PRIVATE glm::glm)
elseif(TARGET glm)
    target_link_libraries(fractal_terrain PRIVATE glm)
elseif(USE_FETCHCONTENT)
    target_include_directories(fractal_terrain SYSTEM PRIVATE
        ${glm_SOURCE_DIR})
else()
    target_include_directories(fractal_terrain SYSTEM PRIVATE
        ${GLM_ROOT}/include)
endif()

if(NOT USE_FETCHCONTENT)
    # 第三方头文件用 SYSTEM 引入，避免 /W4 下被库自身警告刷屏
    target_include_directories(fractal_terrain SYSTEM PRIVATE
        ${GLFW_ROOT}/include)
endif()

# ---------------------------------------------------------------
# 链接库
# ---------------------------------------------------------------
target_link_libraries(fractal_terrain PRIVATE glfw)

if(WIN32)
    target_link_libraries(fractal_terrain PRIVATE opengl32)
elseif(APPLE)
    target_link_libraries(fractal_terrain PRIVATE
        "-framework OpenGL" "-framework Cocoa" "-framework IOKit")
else()
    find_package(OpenGL REQUIRED)
    target_link_libraries(fractal_terrain PRIVATE OpenGL::GL ${CMAKE_DL_LIBS})
endif()

# ---------------------------------------------------------------
# 编译器警告
# ---------------------------------------------------------------
if(MSVC)
    target_compile_options(fractal_terrain PRIVATE /W4 /utf-8 /permissive-)
    # 抑制第三方头文件在 /W4 下的常见噪声
    target_compile_definitions(fractal_terrain PRIVATE
        _CRT_SECURE_NO_WARNINGS
        NOMINMAX)
else()
    target_compile_options(fractal_terrain PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------
# 把 shaders 目录拷到可执行文件旁边，运行时可直接用相对路径加载
# ---------------------------------------------------------------
add_custom_command(TARGET fractal_terrain POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:fractal_terrain>/shaders
    COMMENT "复制 shaders/ 到可执行文件目录")

# ---------------------------------------------------------------
# 提示信息
# ---------------------------------------------------------------
message(STATUS "")
message(STATUS "==========================================")
message(STATUS " fractal_terrain 配置完成")
message(STATUS "  依赖方式   : ${USE_FETCHCONTENT_MSG}")
message(STATUS "  构建类型   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  输出目录   : ${CMAKE_BINARY_DIR}/bin")
message(STATUS "==========================================")
message(STATUS "")
