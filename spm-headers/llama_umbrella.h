//
//  llama_umbrella.h
//  llmfarm_core
//
//  Created by Qi Wang on 2025-01-26.
//

#pragma once

// Umbrella header for SwiftPM: includes all headers in spm-headers.

// ggml headers
#include "ggml.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"
#include "ggml-cpp.h"
#include "ggml-cpu.h"
#include "ggml-metal.h"

// llama
#include "llama.h"

// llava (from examples/llava)
#include "llava.h"

// clip
#include "clip.h"
