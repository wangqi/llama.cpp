llama_add_compile_flags()

#
# libraries
#

# llama

file(GLOB LLAMA_MODELS_SOURCES "models/*.cpp")
file(GLOB LLAMA_CLIP_MODEL_SOURCES "clip-models/*.cpp")

add_library(llama
            ../include/llama.h
            llama.cpp
            llama-adapter.cpp
            llama-arch.cpp
            llama-batch.cpp
            llama-chat.cpp
            llama-context.cpp
            llama-cparams.cpp
            llama-grammar.cpp
            llama-graph.cpp
            llama-hparams.cpp
            llama-impl.cpp
            llama-io.cpp
            llama-kv-cache.cpp
            llama-kv-cache-iswa.cpp
            llama-kv-cache-dsa.cpp
            llama-kv-cache-dsa-iswa.cpp
            llama-kv-cache-msa.cpp
            llama-kv-cache-dsv4.cpp
            llama-memory.cpp
            llama-memory-hybrid.cpp
            llama-memory-hybrid-iswa.cpp
            llama-memory-hybrid-idx.cpp
            llama-memory-recurrent.cpp
            llama-mmap.cpp
            llama-model-loader.cpp
            llama-model-saver.cpp
            llama-model.cpp
            llama-quant.cpp
            llama-sampler.cpp
            llama-vocab.cpp
            unicode-data.cpp
            unicode.cpp
            unicode.h
            ${LLAMA_MODELS_SOURCES}
            clip.cpp
            mtmd.cpp
            mtmd-audio.cpp
            mtmd-image.cpp
            mtmd-helper.cpp
            # mtmd-helper split into a second TU in b10333 (PR #26254, Qwen3-TTS audio gen)
            # wangqi modified 2026-08-10
            mtmd-helper-gen.cpp
            # mtmd-helper.cpp includes "hash/hash.h" since b10724 (PR #27274, sha256 input
            # hashing). Upstream links the separate vendor::hash static library, but the
            # xcframework build only merges libllama/libggml*, so these are compiled straight
            # into the llama target from the copy build-xcframework-ios.sh puts in src/hash/.
            # wangqi modified 2026-08-31
            hash/hash.cpp
            hash/sha256/sha256.c
            hash/sha1/sha1.c
            hash/xxhash/xxhash.c
            ${LLAMA_CLIP_MODEL_SOURCES}
            )

# sha1.h wraps its API in `namespace vendor_hash`, so sha1.c only compiles as C++.
# Matches vendor/hash/CMakeLists.txt. wangqi modified 2026-08-31
set_source_files_properties(hash/sha1/sha1.c PROPERTIES LANGUAGE CXX)

set_target_properties(llama PROPERTIES
    VERSION ${LLAMA_VERSION_BASE}
    SOVERSION ${LLAMA_VERSION_MAJOR}
    MACHO_CURRENT_VERSION 0 # keep macOS linker from seeing oversized version number
)

target_compile_definitions(llama PRIVATE
    LLAMA_VERSION="${LLAMA_VERSION}"
    LLAMA_COMMIT="${LLAMA_BUILD_COMMIT}"
)

target_include_directories(llama PRIVATE .)
# sha256.c includes "rotate-bits/rotate-bits.h" relative to the hash root.
# wangqi modified 2026-08-31
target_include_directories(llama PRIVATE hash)
target_include_directories(llama PUBLIC ../include)
target_compile_features   (llama PRIVATE cxx_std_17) # don't bump

target_link_libraries(llama PUBLIC ggml)

if (BUILD_SHARED_LIBS)
    set_target_properties(llama PROPERTIES POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(llama PRIVATE LLAMA_BUILD)
    target_compile_definitions(llama PUBLIC  LLAMA_SHARED)
endif()
