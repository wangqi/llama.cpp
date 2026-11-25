// llama-app-bridge: see llama-app-bridge.h.
// Fork-owned file: app bridge for llama.cpp audio (TTS / ASR prompts) // wangqi modified 2026-11-24
#include "llama-app-bridge.h"

#include "chat.h"
#include "common.h"
#include "llama.h"
#include "mtmd.h"
#include "../../src/llama-ext.h"

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
