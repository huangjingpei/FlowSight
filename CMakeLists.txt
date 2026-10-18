cmake_minimum_required(VERSION 3.24)
project(FlowSight VERSION 1.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 默认搜索路径 (支持开箱即用)
list(PREPEND CMAKE_PREFIX_PATH
    "C:/Qt/6.9.2/msvc2022_64"
    "D:/opencv/build"
    "D:/vcpkg/installed/x64-windows"
)

# 1. 查找 Qt6 组件
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Multimedia MultimediaWidgets Network)
message(STATUS "Found Qt6: ${Qt6_VERSION} at ${Qt6_DIR}")

# 2. 查找 OpenCV
set(OpenCV_RUNTIME "vc16" CACHE STRING "")
set(OpenCV_DIR "D:/opencv/build/x64/vc16/lib" CACHE PATH "Path to OpenCV cmake")
find_package(OpenCV REQUIRED)
message(STATUS "Found OpenCV: ${OpenCV_VERSION} at ${OpenCV_DIR}")

# 3. 配置 ONNX Runtime
set(ONNXRUNTIME_ROOT_DIR "D:/sherpa-onnx/build-rt/_deps/onnxruntime-src" CACHE PATH "Path to ONNX Runtime SDK")
if(EXISTS "${ONNXRUNTIME_ROOT_DIR}/include/onnxruntime_cxx_api.h")
    message(STATUS "Found ONNX Runtime at ${ONNXRUNTIME_ROOT_DIR}")
else()
    message(WARNING "ONNX Runtime not found at ${ONNXRUNTIME_ROOT_DIR}, please specify -DONNXRUNTIME_ROOT_DIR=...")
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
    ${ONNXRUNTIME_ROOT_DIR}/include
)

link_directories(
    ${ONNXRUNTIME_ROOT_DIR}/lib
)

# 4. 源码文件收集
file(GLOB_RECURSE SRC_COMMON "src/common/*.hpp")
file(GLOB_RECURSE SRC_AI "src/ai/*.cpp" "src/ai/*.hpp")
file(GLOB_RECURSE SRC_TRACKING "src/tracking/*.cpp" "src/tracking/*.hpp")
file(GLOB_RECURSE SRC_ANALYTICS "src/analytics/*.cpp" "src/analytics/*.hpp")
file(GLOB_RECURSE SRC_STORAGE "src/storage/*.cpp" "src/storage/*.hpp")
file(GLOB_RECURSE SRC_NETWORK "src/network/*.cpp" "src/network/*.hpp")
file(GLOB_RECURSE SRC_VIDEO "src/video/*.cpp" "src/video/*.hpp")
file(GLOB_RECURSE SRC_PIPELINE "src/pipeline/*.cpp" "src/pipeline/*.hpp")
file(GLOB_RECURSE SRC_UI "src/ui/*.cpp" "src/ui/*.hpp" "src/ui/views/*.cpp" "src/ui/views/*.hpp" "src/ui/components/*.cpp" "src/ui/components/*.hpp")
file(GLOB_RECURSE SRC_API "src/api/*.cpp" "src/api/*.hpp")

# 5.1 生成独立的 C-ABI 共享动态库 (SDK 核心引擎，0 Qt 依赖)
add_library(FlowSightEngine SHARED
    include/flowsight/FlowSightAPI.h
    ${SRC_API}
    ${SRC_COMMON}
    ${SRC_AI}
    ${SRC_TRACKING}
    ${SRC_ANALYTICS}
)
target_compile_definitions(FlowSightEngine PRIVATE FLOWSIGHT_BUILD_SHARED)
target_link_libraries(FlowSightEngine PRIVATE
    ${OpenCV_LIBS}
    onnxruntime
)

# 5.2 生成可执行程序目标 (GUI 客户端完整套件)
set(APP_ICON_RESOURCE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
set(APP_QRC "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.qrc")

add_executable(FlowSight
    ${SRC_COMMON}
    ${SRC_AI}
    ${SRC_TRACKING}
    ${SRC_ANALYTICS}
    ${SRC_STORAGE}
    ${SRC_NETWORK}
    ${SRC_VIDEO}
    ${SRC_PIPELINE}
    ${SRC_UI}
    ${SRC_API}
    ${APP_ICON_RESOURCE}
    ${APP_QRC}
)

# 6. 链接库
target_link_libraries(FlowSight PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    ${OpenCV_LIBS}
    onnxruntime
)

# 7. 自动拷贝运行时 DLL 到构建目录
if(WIN32)
    # 拷贝 ONNX Runtime DLL 与 Provider 插件
    if(EXISTS "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll")
        add_custom_command(TARGET FlowSight POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:FlowSight>
        )
    endif()
    if(EXISTS "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime_providers_shared.dll")
        add_custom_command(TARGET FlowSight POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime_providers_shared.dll"
            $<TARGET_FILE_DIR:FlowSight>
        )
    endif()
    if(EXISTS "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime_providers_cuda.dll")
        add_custom_command(TARGET FlowSight POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime_providers_cuda.dll"
            $<TARGET_FILE_DIR:FlowSight>
        )
    endif()

    # 拷贝 OpenCV DLL (含 FFmpeg 硬件解码插件)
    if(EXISTS "D:/opencv/build/x64/vc16/bin/opencv_world480.dll")
        add_custom_command(TARGET FlowSight POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "D:/opencv/build/x64/vc16/bin/opencv_world480.dll"
            $<TARGET_FILE_DIR:FlowSight>
        )
    endif()
    if(EXISTS "D:/opencv/build/x64/vc16/bin/opencv_videoio_ffmpeg480_64.dll")
        add_custom_command(TARGET FlowSight POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "D:/opencv/build/x64/vc16/bin/opencv_videoio_ffmpeg480_64.dll"
            $<TARGET_FILE_DIR:FlowSight>
        )
    endif()

    # 针对 FlowSightEngine 独立动态库目标同步拷贝运行时依赖
    if(EXISTS "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll")
        add_custom_command(TARGET FlowSightEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ONNXRUNTIME_ROOT_DIR}/lib/onnxruntime.dll"
            $<TARGET_FILE_DIR:FlowSightEngine>
        )
    endif()
    if(EXISTS "D:/opencv/build/x64/vc16/bin/opencv_world480.dll")
        add_custom_command(TARGET FlowSightEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "D:/opencv/build/x64/vc16/bin/opencv_world480.dll"
            $<TARGET_FILE_DIR:FlowSightEngine>
        )
    endif()
endif()
