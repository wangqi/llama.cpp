# Fork-owned file: static target llama-app-bridge, the plain C shim over libcommon the app links through
# llama.framework (build-xcframework-ios.sh). Added from the root CMakeLists.txt when LLAMA_APP_BRIDGE is ON.
# wangqi modified 2026-11-24
set(TARGET llama-app-bridge)
add_library(${TARGET} STATIC llama-app-bridge.cpp llama-app-bridge.h)
# mtmd.h: the mtmd sources are compiled into libllama by this fork, there is no mtmd target here
target_include_directories(${TARGET} PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(${TARGET} PRIVATE ${PROJECT_SOURCE_DIR}/tools/mtmd)
target_link_libraries     (${TARGET} PUBLIC  llama-common llama)
target_compile_features   (${TARGET} PRIVATE cxx_std_17)
