# Fork-owned file: static target llama-app-bridge, the plain C shim over libcommon the app links through
# llama.framework (build-xcframework-ios.sh). Added from the root CMakeLists.txt when LLAMA_APP_BRIDGE is ON.
# wangqi modified 2026-11-24
set(TARGET llama-app-bridge)
add_library(${TARGET} STATIC llama-app-bridge.cpp llama-app-bridge.h)
target_include_directories(${TARGET} PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
# mtmd.h comes from upstream's mtmd target (tools/mtmd, built standalone with LLAMA_BUILD_MTMD); it is defined after
# this directory is added, which CMake resolves at generate time. The root CMakeLists.txt checks that it is built.
# wangqi modified 2026-11-27
target_link_libraries     (${TARGET} PUBLIC  llama-common llama mtmd)
target_compile_features   (${TARGET} PRIVATE cxx_std_17)
