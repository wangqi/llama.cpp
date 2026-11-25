// llama-app-bridge: a plain C surface over the libcommon helpers the app needs and cannot reach from
// Swift (the "tts" / ASR chat-template presets with their C++-only Jinja builtins and the TTS context
// set-up of common_init_from_params). Compiled into llama.framework by build-xcframework-ios.sh
// (LLAMA_APP_BRIDGE=ON); not part of upstream llama.cpp.
// Fork-owned file: app bridge for llama.cpp audio (TTS / ASR prompts) // wangqi modified 2026-11-24
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct llama_model;
struct llama_context_params;
struct mtmd_context;

// Return codes of the prompt functions
#define LAB_PROMPT_NO_TEMPLATE   0   // the model has no such template: speak / use the raw text
#define LAB_PROMPT_BUF_TOO_SMALL (-1) // out_size too small; nothing useful written (out is NUL terminated if out_size > 0)
#define LAB_PROMPT_ERROR         (-2) // the template threw or an argument was invalid

// Renders the text to speak with the model's "tts" chat template (common_chat_get_tts_prompt), with the
// mmproj's letter-to-sound model as tts_g2p() when mctx is not NULL.
// lang and voice are optional (NULL or "" = the template's default).
// Returns the byte length written to out (> 0, NUL terminated), or one of the LAB_PROMPT_* codes.
int32_t lab_tts_prompt(const struct llama_model * model, struct mtmd_context * mctx,
                       const char * text, const char * lang, const char * voice,
                       char * out, size_t out_size);

// Fully rendered ASR prompt, the same steps as llama-server's /v1/audio/transcriptions
// (convert_transcriptions_to_chatcmpl): the model's ASR preset (queried again with audio_seconds when the
// preset needs the audio length), the user prompt (user_context when the preset does not hold it),
// " (language: X)" when the preset does not name the language, mtmd_default_marker() before or after the
// prompt, system + user messages, chat template with the generation prompt.
// lang and user_context are optional (NULL or ""); audio_seconds 0 = unknown.
// The returned string contains mtmd_default_marker() exactly once: the caller tokenises it with
// mtmd_tokenize and the audio bitmap.
// Returns the byte length written (> 0), LAB_PROMPT_BUF_TOO_SMALL or LAB_PROMPT_ERROR (never 0).
int32_t lab_asr_prompt(const struct llama_model * model, const char * lang, double audio_seconds,
                       const char * user_context, char * out, size_t out_size);

// The transcript in the text the model generated for that prompt (same lang / audio_seconds / user_context):
// llama-server's chat parser for the request (common_chat_session::finish), which drops what the template marks
// as reasoning (e.g. Qwen3-ASR's "language English<asr_text>" prefix) and returns the message content.
// Returns the byte length written (>= 0; 0 = empty transcript), LAB_PROMPT_BUF_TOO_SMALL or LAB_PROMPT_ERROR.
int32_t lab_asr_parse(const struct llama_model * model, const char * lang, double audio_seconds,
                      const char * user_context, const char * generated, char * out, size_t out_size);

// Adjusts context params the way common_init_from_params() does for a TTS / music backbone:
// embeddings = true; a classifier-free-guidance model gets kv_unified = true and n_seq_max *= 2;
// n_batch is capped to n_ubatch (embeddings need the whole batch in one ubatch).
void lab_audio_context_params(const struct llama_model * model, struct llama_context_params * io);

#ifdef __cplusplus
}
#endif
