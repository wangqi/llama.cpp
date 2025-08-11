# What's New in llama.cpp (b5952...b6128)

This document summarizes the key changes in llama.cpp between tags `b5952` and `b6128`.

## Mobile-Related Changes

Several commits focus on improving performance and support for mobile-related hardware and backends:

*   **Metal:** There are several updates to the Metal backend, which is used for Apple devices (iOS, macOS). These include performance improvements for `SSM_SCAN`, general updates, and bug fixes for fusion across different encoders.
*   **OpenCL/Adreno:** A fix for Adreno (Qualcomm mobile GPU) compiler detection in the OpenCL backend.
*   **ARM/CPU:** Deduplication of scalar implementations for various architectures, including ARM.
*   **CANN (Huawei Ascend):** Numerous updates for the CANN backend, which is used for Huawei's Ascend NPUs, often found in mobile devices. These include adding a build pipeline, ACL Graph support, and performance improvements.

## General Updates

This release includes a wide range of new features, model support, and backend improvements:

*   **Model Support:**
    *   Improved Mistral integration (`#14737`)
    *   Support for Intern-S1 (`#14875`)
    *   Support for Granite model reasoning and tool calls (`#14864`)
    *   Support for GPT-OSS (`#15091`)
    *   Support for GLM 4.5 family of models (`#14939`)
    *   Support for LLaDA 8b Diffusion model (`#14771`)
*   **Features:**
    *   Numpy MXFP4 de/quantization support in `gguf-py` (`#15111`)
    *   Basic `SET_ROWS` support in WebGPU (`#15137`)
    *   Universal assisted decoding in the server (`#12635`)
*   **Server:**
    *   Benchmarking against external OpenAI-compatible servers (`#15179`)

For a full list of changes, please refer to the [commit log on GitHub](https://github.com/ggml-org/llama.cpp/compare/b5952...b6128).
