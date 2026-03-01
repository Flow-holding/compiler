# Flow CLI — build con webview cross-platform (Windows/macOS/Linux)
# Uso: cmake -B build -S . && cmake --build build

cmake_minimum_required(VERSION 3.16)
project(flow-cli C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# Webview: usa copia locale in deps/webview
set(WEBVIEW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/webview-tmp")
if(NOT EXISTS "${WEBVIEW_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Webview non trovato. Esegui: git clone --depth 1 https://github.com/webview/webview.git deps/webview-tmp")
endif()

set(WEBVIEW_BUILD_EXAMPLES OFF CACHE BOOL "")
set(WEBVIEW_BUILD_TESTS OFF CACHE BOOL "")
set(WEBVIEW_BUILD_DOCS OFF CACHE BOOL "")
set(WEBVIEW_BUILD_AMALGAMATION OFF CACHE BOOL "")
set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE BOOL "")
set(WEBVIEW_BUILD_STATIC_LIBRARY ON CACHE BOOL "")
add_subdirectory(${WEBVIEW_DIR} ${CMAKE_BINARY_DIR}/webview EXCLUDE_FROM_ALL)

# Flow CLI
add_executable(flow
    main.c util.c config.c flowc.c
    cmd/cmd_init.c cmd/cmd_build.c cmd/cmd_dev.c cmd/cmd_update.c
    webview/wv_webview.c
)

target_include_directories(flow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${WEBVIEW_DIR}/core/include
)

target_link_libraries(flow PRIVATE webview::core_static)

if(WIN32)
    target_link_libraries(flow PRIVATE ws2_32)
    target_compile_definitions(flow PRIVATE _CRT_SECURE_NO_WARNINGS _WIN32_WINNT=0x0A00)
else()
    target_link_libraries(flow PRIVATE pthread dl)
endif()
