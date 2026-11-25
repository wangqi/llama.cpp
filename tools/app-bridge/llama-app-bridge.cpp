// llama-app-bridge: see llama-app-bridge.h.
// Fork-owned file: app bridge for llama.cpp audio (TTS / ASR prompts) // wangqi modified 2026-11-24
#include "llama-app-bridge.h"

#include "chat.h"
#include "common.h"
#include "llama.h"
#include "mtmd.h"
#include "../../src/llama-ext.h"
// speculative decoding (lab_spec_*) // wangqi modified 2026-11-24
#include "speculative.h"
#include "gguf.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>

namespace {

// copies s into out; returns the length, or LAB_PROMPT_BUF_TOO_SMALL
int32_t lab_copy_out(const std::string & s, char * out, size_t out_size) {
    if (out == nullptr || out_size == 0) {
        return LAB_PROMPT_BUF_TOO_SMALL;
    }
    if (s.size() + 1 > out_size) {
        out[0] = '\0';
        return LAB_PROMPT_BUF_TOO_SMALL;
    }
    std::memcpy(out, s.data(), s.size());
    out[s.size()] = '\0';
    return (int32_t) s.size();
}

std::string lab_str(const char * s) {
    return s ? std::string(s) : std::string();
}

} // namespace

extern "C" int32_t lab_tts_prompt(const struct llama_model * model, struct mtmd_context * mctx,
                                  const char * text, const char * lang, const char * voice,
                                  char * out, size_t out_size) {
    if (model == nullptr || text == nullptr) {
        fprintf(stderr, "%s: model and text are required\n", __func__);
        return LAB_PROMPT_ERROR;
    }
    std::string prompt;
    try {
        // the same call sequence as tools/tts/tts.cpp
        auto tmpls = common_chat_templates_init(model, "");
        prompt = common_chat_get_tts_prompt(tmpls.get(), text, lab_str(lang), lab_str(voice),
            [mctx](const std::string & word) {
                char buf[512];
                return mctx && mtmd_gen_audio_g2p(mctx, word.c_str(), buf, sizeof(buf)) > 0 ? std::string(buf) : std::string();
            });
    } catch (const std::exception & e) {
        fprintf(stderr, "%s: tts template failed: %s\n", __func__, e.what());
        return LAB_PROMPT_ERROR;
    }
    if (prompt.empty()) {
        if (out != nullptr && out_size > 0) {
            out[0] = '\0';
        }
        return LAB_PROMPT_NO_TEMPLATE;
    }
    return lab_copy_out(prompt, out, out_size);
}

namespace {

// the chat inputs of a transcription request, the steps of convert_transcriptions_to_chatcmpl
// (tools/server/server-chat.cpp) and the server defaults of oaicompat_chat_params_parse (jinja, generation
// prompt, reasoning format deepseek)
common_chat_templates_inputs lab_asr_inputs(const common_chat_templates * tmpls, const char * lang,
                                            double audio_seconds, const char * user_context) {
    const std::string language = lab_str(lang);
    std::string prompt         = lab_str(user_context);

    common_chat_prompt_preset preset = common_chat_get_asr_prompt(tmpls, language, 0.0, prompt);
    if (preset.needs_audio_length && audio_seconds > 0.0) {
        preset = common_chat_get_asr_prompt(tmpls, language, audio_seconds, prompt);
    }
    if (prompt.empty() || preset.has_context) {
        prompt = preset.user;
    }
    if (!language.empty() && !preset.has_language) {
        prompt += " (language: " + language + ")";
    }
    const std::string marker = mtmd_default_marker();
    prompt = preset.media_first ? marker + prompt : prompt + marker;

    common_chat_templates_inputs inputs;
    inputs.use_jinja             = true;
    inputs.add_generation_prompt = true;
    inputs.reasoning_format      = COMMON_REASONING_FORMAT_DEEPSEEK;
    if (!preset.system.empty()) {
        common_chat_msg sys;
        sys.role    = "system";
        sys.content = preset.system;
        inputs.messages.push_back(sys);
    }
    common_chat_msg user;
    user.role    = "user";
    user.content = prompt;
    inputs.messages.push_back(user);
    return inputs;
}

} // namespace

extern "C" int32_t lab_asr_prompt(const struct llama_model * model, const char * lang, double audio_seconds,
                                  const char * user_context, char * out, size_t out_size) {
    if (model == nullptr) {
        fprintf(stderr, "%s: model is required\n", __func__);
        return LAB_PROMPT_ERROR;
    }
    try {
        auto tmpls = common_chat_templates_init(model, "");
        const auto inputs = lab_asr_inputs(tmpls.get(), lang, audio_seconds, user_context);
        // the prompt llama-server sends (common_chat_session, server-common.cpp)
        common_chat_session session(tmpls.get(), llama_model_get_vocab(model), inputs);
        return lab_copy_out(session.prompt().str(), out, out_size);
    } catch (const std::exception & e) {
        fprintf(stderr, "%s: asr template failed: %s\n", __func__, e.what());
        return LAB_PROMPT_ERROR;
    }
}

extern "C" int32_t lab_asr_parse(const struct llama_model * model, const char * lang, double audio_seconds,
                                 const char * user_context, const char * generated, char * out, size_t out_size) {
    if (model == nullptr || generated == nullptr) {
        fprintf(stderr, "%s: model and generated text are required\n", __func__);
        return LAB_PROMPT_ERROR;
    }
    try {
        auto tmpls = common_chat_templates_init(model, "");
        const auto inputs = lab_asr_inputs(tmpls.get(), lang, audio_seconds, user_context);
        common_chat_session session(tmpls.get(), llama_model_get_vocab(model), inputs);
        const common_chat_msg & msg = session.finish(common_chat_input(std::string(generated)));
        if (msg.content.empty()) {
            if (out == nullptr || out_size == 0) {
                return LAB_PROMPT_BUF_TOO_SMALL;
            }
            out[0] = '\0';
            return 0;
        }
        return lab_copy_out(msg.content, out, out_size);
    } catch (const std::exception & e) {
        fprintf(stderr, "%s: asr output parse failed: %s\n", __func__, e.what());
        return LAB_PROMPT_ERROR;
    }
}

extern "C" void lab_audio_context_params(const struct llama_model * model, struct llama_context_params * io) {
    if (model == nullptr || io == nullptr) {
        fprintf(stderr, "%s: model and params are required\n", __func__);
        return;
    }
    // the TTS rules of common_init_from_params (common/common.cpp); tools/tts/tts.cpp sets embedding = true
    io->embeddings = true;
    if (llama_model_classifier_free_guidance(model)) {
        io->kv_unified = true;
        io->n_seq_max *= 2;
    }
    if (io->n_batch > io->n_ubatch) {
        io->n_batch = io->n_ubatch;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Speculative decoding (lab_spec_*): a transliteration of examples/speculative-simple/speculative-simple.cpp
// (L33-36 output limits, L54-70 drafter set-up, L132-155 prompt feed, L166-345 the step) on the app's own
// llama_sampler chain. Single sequence (seq 0).
// Fork-owned code: app bridge for Mac speculative decoding // wangqi modified 2026-11-24
// ---------------------------------------------------------------------------------------------------------------

namespace {

constexpr llama_seq_id LAB_SPEC_SEQ = 0;
// a re-decode after a checkpoint restore normally accepts everything; bound the loop so a numerically different
// re-decode can never spin
constexpr int LAB_SPEC_MAX_RESTORES_PER_STEP = 8;

void lab_err(char * err, size_t err_size, const std::string & msg) {
    if (err != nullptr && err_size > 0) {
        snprintf(err, err_size, "%s", msg.c_str());
    }
}

// the draft file's architecture; "" when unreadable
std::string lab_gguf_arch(const std::string & path) {
    gguf_init_params gp = { /* .no_alloc = */ true, /* .ctx = */ nullptr };
    gguf_context * g = gguf_init_from_file(path.c_str(), gp);
    if (g == nullptr) {
        return "";
    }
    std::string arch;
    const int64_t id = gguf_find_key(g, "general.architecture");
    if (id >= 0 && gguf_get_kv_type(g, id) == GGUF_TYPE_STRING) {
        arch = gguf_get_val_str(g, id);
    }
    gguf_free(g);
    return arch;
}

bool lab_ends_with(const std::string & s, const std::string & suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The common_speculative type for a mode and draft file.
// MTP: draft-mtp on the target's weights. DRAFT: an "*-assistant" architecture (gemma4-assistant) is an MTP head that
// shares the target's KV and only runs as draft-mtp (P0b ran it as `-md <assistant> --spec-type draft-mtp`); any
// other file is a standalone model and runs as draft-simple, even when it carries NextN layers of its own (the
// Qwen3.5-0.8B draft does: upstream's GGUF auto-detection, common_speculative_types_from_gguf, would wrongly pick
// draft-mtp for it). DFlash-family drafts ("dflash") are not offered.
common_speculative_type lab_spec_type(const lab_spec_params & p, std::string & why) {
    if (p.mode == LAB_SPEC_MODE_MTP) {
        return COMMON_SPECULATIVE_TYPE_DRAFT_MTP;
    }
    if (p.mode != LAB_SPEC_MODE_DRAFT) {
        why = "speculative mode off";
        return COMMON_SPECULATIVE_TYPE_NONE;
    }
    const std::string path = lab_str(p.draft_path);
    if (path.empty()) {
        why = "draft mode needs a draft model path";
        return COMMON_SPECULATIVE_TYPE_NONE;
    }
    const std::string arch = lab_gguf_arch(path);
    if (arch.empty()) {
        why = "cannot read the draft model's architecture: " + path;
        return COMMON_SPECULATIVE_TYPE_NONE;
    }
    if (arch == "dflash") {
        why = "DFlash-family draft models are not supported";
        return COMMON_SPECULATIVE_TYPE_NONE;
    }
    if (lab_ends_with(arch, "-assistant")) {
        return COMMON_SPECULATIVE_TYPE_DRAFT_MTP;
    }
    return COMMON_SPECULATIVE_TYPE_DRAFT_SIMPLE;
}

int32_t lab_spec_n_max(const lab_spec_params & p) {
    return p.n_max > 0 ? p.n_max : 3;
}

} // namespace

struct lab_spec {
    common_params                      params;     // the target's params as common sees them (n_ctx, batch, threads, speculative)
    common_speculative_init_result_ptr spec_init;  // owns the draft model / context
    common_speculative               * spec    = nullptr;
    llama_context                    * ctx_tgt = nullptr;
    llama_context                    * ctx_dft = nullptr;
    common_speculative_type            type    = COMMON_SPECULATIVE_TYPE_NONE;

    bool use_ckpt_tgt = false;
    bool use_ckpt_dft = false;
    bool need_begin   = true;

    llama_tokens prompt_tgt; // every token in the target context, by position (LLAMA_TOKEN_NULL for media positions)
    llama_tokens draft;
    common_prompt_checkpoint ckpt;
    common_batch batch_tgt;
    common_batch batch_in;   // the app's llama_batch, mirrored for common_speculative_process

    lab_spec_stats stats = {};

    ~lab_spec() {
        if (spec != nullptr) {
            common_speculative_free(spec);
            spec = nullptr;
        }
        spec_init.reset(); // draft context, then draft model
    }
};

extern "C" struct lab_spec_params lab_spec_default_params(void) {
    lab_spec_params p = {};
    p.mode       = LAB_SPEC_MODE_OFF;
    p.n_max      = 3;   // common_params_speculative_draft defaults
    p.n_min      = 0;
    p.p_min      = 0.0f;
    p.draft_path = nullptr;
    p.draft_ngl  = -1;
    return p;
}

extern "C" void lab_spec_model_params(const struct lab_spec_params * p, struct llama_model_params * io) {
    if (p == nullptr || io == nullptr) {
        fprintf(stderr, "%s: params are required\n", __func__);
        return;
    }
    // common_model_params_to_llama: load_mtp when draft-mtp runs on the target's own NextN layers
    if (p->mode == LAB_SPEC_MODE_MTP) {
        io->load_mtp = true;
    }
}

extern "C" void lab_spec_context_params(const struct lab_spec_params * p, const struct llama_model * tgt,
                                        struct llama_context_params * io) {
    if (p == nullptr || io == nullptr || tgt == nullptr) {
        fprintf(stderr, "%s: params and model are required\n", __func__);
        return;
    }
    if (p->mode == LAB_SPEC_MODE_OFF) {
        return;
    }
    const int32_t n_max = lab_spec_n_max(*p);
    // common_context_params_to_llama: n_rs_seq = need_n_rs_seq() (n_max for the MTP family, 0 for draft-simple)
    std::string why;
    const common_speculative_type type = lab_spec_type(*p, why);
    if (type == COMMON_SPECULATIVE_TYPE_DRAFT_MTP) {
        io->n_rs_seq = (uint32_t) n_max;
    }
    // speculative-simple.cpp:33-36
    const auto limits = common_speculative_get_output_limits((int32_t) io->n_batch, 1, n_max);
    io->n_outputs_max         = limits.total;
    io->n_outputs_max_per_seq = limits.per_seq;
}

extern "C" struct lab_spec * lab_spec_init(struct llama_model * tgt, struct llama_context * ctx_tgt,
                                           const struct lab_spec_params * p, char * err, size_t err_size) {
    if (tgt == nullptr || ctx_tgt == nullptr || p == nullptr) {
        lab_err(err, err_size, "model, context and params are required");
        return nullptr;
    }
    std::string why;
    const common_speculative_type type = lab_spec_type(*p, why);
    if (type == COMMON_SPECULATIVE_TYPE_NONE) {
        lab_err(err, err_size, why);
        return nullptr;
    }
    if (p->mode == LAB_SPEC_MODE_MTP && llama_model_n_layer_nextn(tgt) <= 0) {
        lab_err(err, err_size, "the model has no NextN (MTP) layers, or they were not loaded (load_mtp)");
        return nullptr;
    }

    std::unique_ptr<lab_spec> s(new lab_spec());
    try {
        auto & cp = s->params;
        // the target as common_init_from_params would have set it up; the draft context copies these
        // (common_base_params_to_speculative)
        cp.n_ctx      = (int32_t) llama_n_ctx(ctx_tgt);
        cp.n_batch    = (int32_t) llama_n_batch(ctx_tgt);
        cp.n_ubatch   = (int32_t) llama_n_ubatch(ctx_tgt);
        cp.n_parallel = 1;
        cp.cpuparams.n_threads       = llama_n_threads(ctx_tgt);
        cp.cpuparams_batch.n_threads = llama_n_threads_batch(ctx_tgt);
        cp.fit_params = false;
        cp.speculative.types       = { type };
        cp.speculative.draft.n_max = lab_spec_n_max(*p);
        cp.speculative.draft.n_min = std::max(0, p->n_min);
        cp.speculative.draft.p_min = p->p_min;
        if (p->mode == LAB_SPEC_MODE_DRAFT) {
            cp.speculative.draft.mparams.path = lab_str(p->draft_path);
            cp.speculative.draft.n_gpu_layers = p->draft_ngl;
        }

        // speculative-simple.cpp:54-66
        common_params params_dft = common_base_params_to_speculative(cp);
        s->spec_init = common_speculative_init_from_params(params_dft, tgt, ctx_tgt);
        s->ctx_dft   = s->spec_init->context();
        if (s->ctx_dft == nullptr) {
            lab_err(err, err_size, p->mode == LAB_SPEC_MODE_DRAFT
                    ? "failed to load the draft model or create its context: " + lab_str(p->draft_path)
                    : "failed to create the MTP context");
            return nullptr;
        }
        if (p->mode == LAB_SPEC_MODE_DRAFT && type == COMMON_SPECULATIVE_TYPE_DRAFT_SIMPLE &&
            !common_speculative_are_compatible(tgt, s->spec_init->model())) {
            lab_err(err, err_size, "the draft model's vocabulary does not match the target's");
            return nullptr;
        }
        cp.speculative.draft.ctx_tgt = ctx_tgt;
        cp.speculative.draft.ctx_dft = s->ctx_dft;

        // speculative-simple.cpp:69-70 (the probe clears the target memory: nothing may be decoded on it yet)
        s->use_ckpt_tgt = common_context_can_seq_rm(ctx_tgt)    == COMMON_CONTEXT_SEQ_RM_TYPE_FULL;
        s->use_ckpt_dft = common_context_can_seq_rm(s->ctx_dft) == COMMON_CONTEXT_SEQ_RM_TYPE_FULL;

        // speculative-simple.cpp:119-129
        s->spec = common_speculative_init(cp.speculative, 1);
        if (s->spec == nullptr) {
            lab_err(err, err_size, "failed to initialize speculative decoding");
            return nullptr;
        }
    } catch (const std::exception & e) {
        lab_err(err, err_size, std::string("speculative init failed: ") + e.what());
        return nullptr;
    }

    s->ctx_tgt   = ctx_tgt;
    s->type      = type;
    s->batch_tgt = common_batch(ctx_tgt);
    s->batch_in  = common_batch(ctx_tgt);
    s->prompt_tgt.reserve(llama_n_ctx(ctx_tgt));
    s->stats.type         = (int32_t) type;
    s->stats.use_ckpt_tgt = s->use_ckpt_tgt ? 1 : 0;
    s->stats.use_ckpt_dft = s->use_ckpt_dft ? 1 : 0;
    s->stats.n_max        = common_speculative_n_max(s->spec);
    return s.release();
}

extern "C" void lab_spec_free(struct lab_spec * spec) {
    delete spec;
}

extern "C" int32_t lab_spec_process_batch(struct lab_spec * spec, const struct llama_batch * batch) {
    if (spec == nullptr || batch == nullptr || batch->token == nullptr || batch->n_tokens <= 0) {
        return LAB_SPEC_ERR_ARGS;
    }
    // speculative-simple.cpp:132-144, with the app's own batch (token entries only; mtmd media is decoded through
    // mtmd_helper_eval_chunks and never reaches here, the app runs such turns without speculation)
    auto & b = spec->batch_in;
    b.clear();
    for (int32_t i = 0; i < batch->n_tokens; ++i) {
        const llama_pos    pos = batch->pos ? batch->pos[i] : (llama_pos) spec->prompt_tgt.size() + i;
        const llama_seq_id sid = (batch->seq_id && batch->n_seq_id && batch->n_seq_id[i] > 0) ? batch->seq_id[i][0] : LAB_SPEC_SEQ;
        const bool         out = batch->logits ? batch->logits[i] != 0 : (i == batch->n_tokens - 1);
        b.add(batch->token[i], pos, sid, out);
        if (sid == LAB_SPEC_SEQ && pos >= 0) {
            if ((size_t) pos < spec->prompt_tgt.size()) {
                spec->prompt_tgt.resize(pos);
            }
            while ((llama_pos) spec->prompt_tgt.size() < pos) {
                spec->prompt_tgt.push_back(LLAMA_TOKEN_NULL);
            }
            spec->prompt_tgt.push_back(batch->token[i]);
        }
    }
    try {
        if (!common_speculative_process(spec->spec, b)) {
            fprintf(stderr, "%s: failed to process the batch on the drafter\n", __func__);
            return LAB_SPEC_ERR_PROCESS;
        }
    } catch (const std::exception & e) {
        fprintf(stderr, "%s: %s\n", __func__, e.what());
        return LAB_SPEC_ERR_PROCESS;
    }
    spec->need_begin = true;
    return 0;
}

extern "C" void lab_spec_seq_rm(struct lab_spec * spec, int32_t p0, int32_t p1) {
    if (spec == nullptr) {
        return;
    }
    if (spec->ctx_dft != nullptr) {
        llama_memory_seq_rm(llama_get_memory(spec->ctx_dft), LAB_SPEC_SEQ, p0 < 0 ? -1 : p0, p0 < 0 ? -1 : p1);
    }
    if (p0 < 0) {
        spec->prompt_tgt.clear();
    } else if (p1 < 0) {
        if ((size_t) p0 < spec->prompt_tgt.size()) {
            spec->prompt_tgt.resize(p0);
        }
    } else if ((size_t) p0 < spec->prompt_tgt.size()) {
        // a hole in the middle: positions shift on the target, the token list no longer maps; drop the tail
        spec->prompt_tgt.resize(p0);
    }
    spec->draft.clear();
    spec->ckpt.clear();
    spec->need_begin = true;
}

extern "C" int32_t lab_spec_step(struct lab_spec * spec, struct llama_context * ctx_tgt, struct llama_sampler * chain,
                                 int32_t id_last, int32_t n_past, int32_t n_budget, int32_t * out_ids, int32_t cap) {
    if (spec == nullptr || spec->spec == nullptr || ctx_tgt == nullptr || ctx_tgt != spec->ctx_tgt ||
        chain == nullptr || out_ids == nullptr || n_past < 0) {
        return LAB_SPEC_ERR_ARGS;
    }
    if (cap < spec->stats.n_max + 1) {
        return LAB_SPEC_ERR_CAP;
    }
    llama_context * ctx_dft = spec->ctx_dft;
    auto & prompt_tgt = spec->prompt_tgt;
    auto & draft      = spec->draft;
    auto & ckpt       = spec->ckpt;
    auto & batch_tgt  = spec->batch_tgt;
    const llama_vocab * vocab = llama_model_get_vocab(llama_get_model(ctx_tgt));

    // the target KV holds n_past positions; the drafter's token list follows it (id_last is not in it yet)
    if ((size_t) n_past < prompt_tgt.size()) {
        prompt_tgt.resize(n_past);
    }
    while ((int32_t) prompt_tgt.size() < n_past) {
        prompt_tgt.push_back(LLAMA_TOKEN_NULL);
    }
    if (spec->need_begin) {
        // speculative-simple.cpp:155, once per run of target decodes the drafter has seen
        common_speculative_begin(spec->spec, LAB_SPEC_SEQ, prompt_tgt);
        spec->need_begin = false;
    }
    draft.clear();

    llama_pos pos = n_past;
    int restores = 0;

    try {
        while (true) {
            // speculative-simple.cpp:175-224
            if (draft.empty()) {
                ckpt.update_pos(
                        prompt_tgt.size(),
                        llama_memory_seq_pos_min(llama_get_memory(ctx_tgt), LAB_SPEC_SEQ),
                        llama_memory_seq_pos_max(llama_get_memory(ctx_tgt), LAB_SPEC_SEQ));

                if (spec->use_ckpt_dft) {
                    ckpt.update_dft(ctx_dft, LAB_SPEC_SEQ, LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY);
                }

                // the max draft that fits the remaining context and the caller's budget
                int n_draft_max = (int) llama_n_ctx(ctx_tgt) - pos - 2;
                if (n_budget >= 0) {
                    n_draft_max = std::min(n_draft_max, n_budget - 1);
                }
                n_draft_max = std::max(n_draft_max, 0);

                if (n_draft_max > 0) {
                    common_speculative_get_draft_params(spec->spec, LAB_SPEC_SEQ) = {
                        /* .drafting   = */ true,
                        /* .n_max      = */ n_draft_max,
                        /* .pos0       = */ pos,
                        /* .id_last    = */ id_last,
                        /* .prompt     = */ &prompt_tgt,
                        /* .result     = */ &draft,
                    };
                    common_speculative_draft(spec->spec);
                }
                if ((int) draft.size() > n_draft_max) {
                    draft.resize(n_draft_max);
                }

                if (!draft.empty() && spec->use_ckpt_tgt) {
                    ckpt.update_tgt(ctx_tgt, LAB_SPEC_SEQ, LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY);
                }

                if (ctx_dft) {
                    if (spec->use_ckpt_dft && !ckpt.load_dft(ctx_dft, LAB_SPEC_SEQ, LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY)) {
                        fprintf(stderr, "%s: draft checkpoint restore failed\n", __func__);
                        return LAB_SPEC_ERR_CKPT;
                    }
                    llama_memory_seq_rm(llama_get_memory(ctx_dft), LAB_SPEC_SEQ, ckpt.pos_max + 1, -1);
                }
            } else if (spec->use_ckpt_tgt && ckpt.empty()) {
                return LAB_SPEC_ERR_CKPT;
            }

            // speculative-simple.cpp:227-238: [id_last, draft0, ..., draftN-1], every row an output
            batch_tgt.clear();
            batch_tgt.add(id_last, pos, LAB_SPEC_SEQ, true);
            for (size_t i = 0; i < draft.size(); ++i) {
                batch_tgt.add(draft[i], pos + 1 + (llama_pos) i, LAB_SPEC_SEQ, true);
            }
            const int32_t rc = llama_process(ctx_tgt, LLAMA_PROCESS_TYPE_DECODE, batch_tgt.get());
            if (rc != 0) {
                fprintf(stderr, "%s: target decode failed rc=%d at pos %d\n", __func__, rc, (int) pos);
                return LAB_SPEC_ERR_DECODE;
            }

            // speculative-simple.cpp:241-244
            if (!common_speculative_process(spec->spec, batch_tgt)) {
                fprintf(stderr, "%s: failed to process the verify batch on the drafter\n", __func__);
                return LAB_SPEC_ERR_PROCESS;
            }

            // speculative-simple.cpp:247-250: keep the chain's state when a partial acceptance may roll it back
            llama_sampler * chain_save = spec->use_ckpt_tgt ? llama_sampler_clone(chain) : nullptr;
            if (spec->use_ckpt_tgt && chain_save == nullptr) {
                fprintf(stderr, "%s: the sampler chain cannot be cloned; no rollback possible\n", __func__);
                return LAB_SPEC_ERR_ARGS;
            }

            const size_t n_draft = draft.size();

            // common_sampler_sample_and_accept_n (common/sampling.cpp:698-730) on the app's chain:
            // llama_sampler_sample applies the chain to row i and accepts the token into it
            llama_tokens ids;
            ids.reserve(n_draft + 1);
            size_t i = 0;
            for (; i < n_draft; ++i) {
                const llama_token id = llama_sampler_sample(chain, ctx_tgt, (int32_t) i);
                ids.push_back(id);
                // no draft token after an EOG: it would stay in the context without being output
                if (draft[i] != id || (llama_vocab_is_eog(vocab, id) && i + 1 < n_draft)) {
                    break;
                }
            }
            if (i == n_draft) {
                ids.push_back(llama_sampler_sample(chain, ctx_tgt, (int32_t) i));
            }

            // speculative-simple.cpp:271-294: partial acceptance on a context without partial seq_rm
            if (spec->use_ckpt_tgt && ids.size() - 1 < n_draft) {
                if (++restores > LAB_SPEC_MAX_RESTORES_PER_STEP) {
                    if (chain_save) {
                        llama_sampler_free(chain_save);
                    }
                    fprintf(stderr, "%s: the step did not settle after %d restores\n", __func__, restores - 1);
                    return LAB_SPEC_ERR_CKPT;
                }
                spec->stats.n_restores++;

                draft = std::move(ids);

                if (!ckpt.load_tgt(ctx_tgt, LAB_SPEC_SEQ, LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY)) {
                    if (chain_save) {
                        llama_sampler_free(chain_save);
                    }
                    fprintf(stderr, "%s: target checkpoint restore failed\n", __func__);
                    return LAB_SPEC_ERR_CKPT;
                }
                llama_memory_seq_rm(llama_get_memory(ctx_tgt), LAB_SPEC_SEQ, ckpt.pos_max + 1, -1);

                if (ctx_dft) {
                    if (!ckpt.load_dft(ctx_dft, LAB_SPEC_SEQ, LLAMA_STATE_SEQ_FLAGS_PARTIAL_ONLY)) {
                        if (chain_save) {
                            llama_sampler_free(chain_save);
                        }
                        fprintf(stderr, "%s: draft checkpoint restore failed\n", __func__);
                        return LAB_SPEC_ERR_CKPT;
                    }
                    llama_memory_seq_rm(llama_get_memory(ctx_dft), LAB_SPEC_SEQ, ckpt.pos_max + 1, -1);
                }

                prompt_tgt.resize(ckpt.n_tokens);

                // the reference swaps its sampler object for the saved clone (smpl = std::move(smpl_save)); the
                // app owns its chain pointer, so the saved state moves into it instead: both are the same sampler
                // type (iface), only the state (ctx) is exchanged, then the advanced state is freed
                if (chain_save) {
                    std::swap(chain->ctx, chain_save->ctx);
                    llama_sampler_free(chain_save);
                }

                pos = (llama_pos) prompt_tgt.size();
                continue;
            }
            if (chain_save) {
                llama_sampler_free(chain_save);
            }

            // speculative-simple.cpp:296-341: full acceptance
            common_speculative_accept(spec->spec, LAB_SPEC_SEQ, (uint16_t) (ids.size() - 1));

            spec->stats.n_steps++;
            spec->stats.n_drafted  += (int64_t) n_draft;
            spec->stats.n_accepted += (int64_t) ids.size() - 1;

            for (size_t k = 0; k < ids.size(); ++k) {
                prompt_tgt.push_back(id_last);
                id_last = ids[k];
            }
            pos = (llama_pos) prompt_tgt.size();

            draft.clear();

            llama_memory_seq_rm(llama_get_memory(ctx_tgt), LAB_SPEC_SEQ, pos, -1);
            if (ctx_dft) {
                llama_memory_seq_rm(llama_get_memory(ctx_dft), LAB_SPEC_SEQ, pos, -1);
            }

            if ((int32_t) ids.size() > cap) {
                return LAB_SPEC_ERR_CAP;
            }
            for (size_t k = 0; k < ids.size(); ++k) {
                out_ids[k] = ids[k];
            }
            return (int32_t) ids.size();
        }
    } catch (const std::exception & e) {
        fprintf(stderr, "%s: %s\n", __func__, e.what());
        return LAB_SPEC_ERR_PROCESS;
    }
}

extern "C" void lab_spec_get_stats(const struct lab_spec * spec, struct lab_spec_stats * out) {
    if (out == nullptr) {
        return;
    }
    if (spec == nullptr) {
        *out = {};
        return;
    }
    *out = spec->stats;
}
