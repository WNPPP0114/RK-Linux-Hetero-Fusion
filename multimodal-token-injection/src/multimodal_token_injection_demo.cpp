/**
 * 端侧多模态推理（Token Injection）Demo
 * - 部署 Qwen3-1.7B（W8A8），语义 Token 直通：YOLO 类别 ID -> LLM 词表 ID 直接映射
 * - 跳过 Tokenizer 与 JSON，使用 RKLLM_INPUT_TOKEN 输入，目标推理速度 ~14 Tokens/s
 *
 * 用法:
 *   仅 LLM + Token 注入（不跑 YOLO）: ./multimodal_token_injection_demo <llm_model.rkllm> <max_new_tokens> <max_context_len> [class_id0 class_id1 ...]
 *   若提供 class_id 列表，则直接将这些类别对应的 token 注入 LLM；否则使用默认示例类别。
 *
 * 依赖: rkllm runtime，以及本仓库生成的 yolo_qwen_token_map.h（含 PREFIX/SUFFIX 时为纯 Token 直通）
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <chrono>
#include <csignal>
#include <vector>
#include <set>

#include "rkllm.h"
#include "yolo_qwen_token_map.h"

static LLMHandle llmHandle = nullptr;
static int g_token_count = 0;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_start_time;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_first_token_time;
static bool g_first_token = true;

void sig_exit(int sig) {
    if (llmHandle) {
        LLMHandle h = llmHandle;
        llmHandle = nullptr;
        rkllm_destroy(h);
    }
    exit(sig);
}

int callback(RKLLMResult *result, void *, LLMCallState state) {
    if (state == RKLLM_RUN_FINISH) {
        auto end = std::chrono::high_resolution_clock::now();
        double total_s = std::chrono::duration<double>(end - g_start_time).count();
        double ttft_s = std::chrono::duration<double>(g_first_token_time - g_start_time).count();
        double decode_s = (g_token_count > 1 && total_s > ttft_s) ? (g_token_count - 1) / (total_s - ttft_s) : 0.0;
        printf("\n[Token Injection] Total: %.4fs, TTFT: %.4fs, Tokens: %d, E2E: %.2f tok/s, Decode: %.2f tok/s\n",
               total_s, ttft_s, g_token_count,
               total_s > 0 ? g_token_count / total_s : 0.0f, decode_s);
    } else if (state == RKLLM_RUN_NORMAL) {
        if (g_first_token) {
            g_first_token_time = std::chrono::high_resolution_clock::now();
            g_first_token = false;
        }
        g_token_count++;
        printf("%s", result->text);
    }
    return 0;
}

/** 将 YOLO 类别 ID 列表转为 Qwen token id 数组（去重、顺序），写入 out_ids，返回长度 */
static int build_detection_token_ids(const std::vector<int>& class_ids, unsigned int* out_ids, int max_out) {
    std::set<unsigned int> seen;
    int n = 0;
    for (int cid : class_ids) {
        if (cid < 0 || cid >= YOLO_COCO_NUM_CLASSES || n >= max_out) continue;
        unsigned int tid = YOLO_CLASS_TO_QWEN_TOKEN_ID[cid];
        if (tid != 0 && seen.find(tid) == seen.end()) {
            seen.insert(tid);
            out_ids[n++] = tid;
        }
    }
    return n;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <llm_model.rkllm> <max_new_tokens> <max_context_len> [class_id0 class_id1 ...]\n", argv[0]);
        fprintf(stderr, "  class_id: 0..79 COCO class, e.g. 0=person 1=bicycle 2=car. Omit for default demo.\n");
        return 1;
    }
    signal(SIGINT, sig_exit);

    const char* model_path = argv[1];
    int max_new_tokens = atoi(argv[2]);
    int max_context_len = atoi(argv[3]);

    std::vector<int> class_ids;
    for (int i = 4; i < argc; i++)
        class_ids.push_back(atoi(argv[i]));
    if (class_ids.empty()) {
        class_ids = {0, 1, 2, 15, 16};
        printf("[Demo] No class_ids given, using default: person,bicycle,car,cat,dog (0,1,2,15,16)\n");
    }

    RKLLMParam param = rkllm_createDefaultParam();
    param.model_path = model_path;
    param.max_new_tokens = max_new_tokens;
    param.max_context_len = max_context_len;
    param.top_k = 1;
    param.top_p = 0.95f;
    param.temperature = 0.8f;
    param.repeat_penalty = 1.1f;
    param.skip_special_token = 1;

    int ret = rkllm_init(&llmHandle, &param, callback);
    if (ret != 0) {
        fprintf(stderr, "rkllm_init failed: %d\n", ret);
        return 1;
    }

    const int max_det_tokens = 64;
    unsigned int det_token_ids[max_det_tokens];
    int num_det = build_detection_token_ids(class_ids, det_token_ids, max_det_tokens);

    /* 构建完整 token 序列: prefix + 检测 token + suffix，直通 LLM，无 Tokenizer/JSON */
    std::vector<int32_t> full_ids;
    full_ids.reserve((size_t)(PREFIX_TOKEN_LEN + num_det + SUFFIX_TOKEN_LEN + 8));
    for (int i = 0; i < PREFIX_TOKEN_LEN; i++)
        full_ids.push_back((int32_t)PREFIX_TOKEN_IDS[i]);
    for (int i = 0; i < num_det; i++)
        full_ids.push_back((int32_t)det_token_ids[i]);
    for (int i = 0; i < SUFFIX_TOKEN_LEN; i++)
        full_ids.push_back((int32_t)SUFFIX_TOKEN_IDS[i]);

    if (full_ids.empty()) {
        fprintf(stderr, "PREFIX_TOKEN_LEN and SUFFIX_TOKEN_LEN are 0. Run script with transformers to generate.\n");
        fprintf(stderr, "Fallback: using prompt mode (no token injection).\n");
        rkllm_destroy(llmHandle);
        return 1;
    }

    RKLLMInput rkllm_input;
    memset(&rkllm_input, 0, sizeof(rkllm_input));
    rkllm_input.input_type = RKLLM_INPUT_TOKEN;
    rkllm_input.token_input.input_ids = full_ids.data();
    rkllm_input.token_input.n_tokens = full_ids.size();

    RKLLMInferParam infer_param;
    memset(&infer_param, 0, sizeof(infer_param));
    infer_param.mode = RKLLM_INFER_GENERATE;

    g_token_count = 0;
    g_first_token = true;
    g_start_time = std::chrono::high_resolution_clock::now();
    printf("[Token Injection] Running with %zu input tokens (prefix + %d detection + suffix)\n", full_ids.size(), num_det);
    printf("assistant: ");
    rkllm_run(llmHandle, &rkllm_input, &infer_param, nullptr);
    rkllm_destroy(llmHandle);
    return 0;
}
