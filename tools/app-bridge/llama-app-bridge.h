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
// speculative decoding (lab_spec_*) // wangqi modified 2026-11-24
struct llama_model_params;
struct llama_context;
struct llama_batch;
struct llama_sampler;

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

// Speculative decoding: upstream's common_speculative draft / verify / accept / rollback run here, one call per
// step, on the app's own llama_sampler chain (grammar, penalties and every user setting stay the app's). A
// transliteration of examples/speculative-simple/speculative-simple.cpp L166-345. Single sequence (seq 0).
// Fork-owned API: app bridge for Mac speculative decoding // wangqi modified 2026-11-24

#define LAB_SPEC_MODE_OFF   0  // no speculation
#define LAB_SPEC_MODE_MTP   1  // the target's own NextN (MTP) layers draft (draft-mtp on the target's weights)
#define LAB_SPEC_MODE_DRAFT 2  // a separate draft GGUF: an "*-assistant" architecture (e.g. gemma4-assistant) runs as
                               // draft-mtp sharing the target's KV; any other model runs as draft-simple

#define LAB_SPEC_ERR_ARGS    (-1) // invalid argument / spec not initialised
#define LAB_SPEC_ERR_DECODE  (-2) // llama_process on the target failed (the target KV may hold a partial batch)
#define LAB_SPEC_ERR_PROCESS (-3) // the drafter could not process the batch
#define LAB_SPEC_ERR_CKPT    (-4) // a checkpoint could not be restored, or the step did not settle
#define LAB_SPEC_ERR_CAP     (-5) // out_ids cap smaller than n_max + 1

struct lab_spec;

struct lab_spec_params {
    int32_t      mode;       // LAB_SPEC_MODE_*
    int32_t      n_max;      // max draft tokens per step (upstream default 3)
    int32_t      n_min;      // drafts shorter than this are dropped (0)
    float        p_min;      // min draft probability (0.0)
    const char * draft_path; // LAB_SPEC_MODE_DRAFT: the draft GGUF
    int32_t      draft_ngl;  // GPU layers of the draft model (-1 = default)
};

struct lab_spec_stats {
    int64_t n_steps;     // lab_spec_step calls that returned tokens
    int64_t n_drafted;   // draft tokens verified (full-acceptance steps, as speculative-simple counts)
    int64_t n_accepted;  // draft tokens accepted
    int64_t n_restores;  // checkpoint restores (partial acceptance on a context without partial seq_rm)
    int32_t type;        // the common_speculative_type in use (0 before init)
    int32_t use_ckpt_tgt; // 1 when the target needs checkpoints
    int32_t use_ckpt_dft; // 1 when the draft context needs checkpoints
    int32_t n_max;       // effective max draft
};

struct lab_spec_params lab_spec_default_params(void);

// Before the target model is loaded: load_mtp = true for LAB_SPEC_MODE_MTP.
void lab_spec_model_params(const struct lab_spec_params * p, struct llama_model_params * io);

// Before the target context is created: n_rs_seq (MTP: recurrent-state snapshots for partial rollback) and
// n_outputs_max / n_outputs_max_per_seq from common_speculative_get_output_limits(n_batch, 1, n_max), so the verify
// batch gets n_max + 1 logit rows (speculative-simple.cpp:33-36).
void lab_spec_context_params(const struct lab_spec_params * p, const struct llama_model * tgt,
                             struct llama_context_params * io);

// After the target context exists and BEFORE anything is decoded on it (the seq_rm probe of a context clears its
// memory). Loads the draft model (LAB_SPEC_MODE_DRAFT) and creates the draft / MTP context. Returns NULL and writes
// a reason to err on an incompatible draft (vocab, no NextN layers, unsupported draft architecture).
struct lab_spec * lab_spec_init(struct llama_model * tgt, struct llama_context * ctx_tgt,
                                const struct lab_spec_params * p, char * err, size_t err_size);
void lab_spec_free(struct lab_spec * spec);

// Feed a batch the app has just decoded on the target (prompt chunks and plain single-token decodes alike), right
// after its llama_decode and before the next decode on the target (speculative-simple.cpp:132-140).
// Returns 0, or LAB_SPEC_ERR_PROCESS.
int32_t lab_spec_process_batch(struct lab_spec * spec, const struct llama_batch * batch);

// Keep the drafter's memory in step when the app trims the target KV of seq 0: [p0, p1), p1 < 0 = to the end,
// p0 < 0 = everything.
void lab_spec_seq_rm(struct lab_spec * spec, int32_t p0, int32_t p1);

// One speculative step. id_last is the token the app sampled last and has NOT decoded; n_past the number of
// positions in the target KV (id_last goes at n_past). n_budget caps the tokens this step may return (-1 = none).
// Drafts, decodes [id_last, drafts...] on the target, samples with llama_sampler_sample(chain, ctx, i) and keeps
// the drafts the chain agrees with; on a partial acceptance with checkpoints it restores the target KV, the draft
// state and the chain, and re-decodes the accepted run inside this call. On return the target KV holds n_past +
// count positions (id_last and the accepted drafts); out_ids[count - 1] is sampled and not decoded (the next
// id_last). Every returned token was accepted by the chain. Returns count (>= 1) or LAB_SPEC_ERR_*.
int32_t lab_spec_step(struct lab_spec * spec, struct llama_context * ctx_tgt, struct llama_sampler * chain,
                      int32_t id_last, int32_t n_past, int32_t n_budget, int32_t * out_ids, int32_t cap);

void lab_spec_get_stats(const struct lab_spec * spec, struct lab_spec_stats * out);

#ifdef __cplusplus
}
#endif
