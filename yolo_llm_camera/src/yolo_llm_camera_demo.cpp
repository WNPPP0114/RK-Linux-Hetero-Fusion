/**
 * YOLO + LLM 摄像头联调：你问 LLM「摄像头看到了什么」，LLM 根据当前画面 YOLO 检测结果回答。
 * 用法: ./yolo_llm_camera_demo <yolo.rknn> <llm.rkllm> [摄像头设备或视频] [max_new_tokens] [max_context_len]
 * 例:   ./yolo_llm_camera_demo model/yolo26n.rknn Qwen3-1.7B_W8A8_RK3588.rkllm 0 256 4096
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>
#include <csignal>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>
#include <deque>
#include <atomic>

#include "opencv2/core.hpp"
#include "opencv2/videoio.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"

#include "rknnPool.hpp"
#include "NpuCoreScheduler.hpp"
#include "rkllm.h"

#ifndef INFERENCE_THREAD_NUM
#define INFERENCE_THREAD_NUM  6
#endif

static LLMHandle g_llm_handle = nullptr;
static int g_token_count = 0;
static bool g_llm_running = false;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_start_time;
static std::chrono::time_point<std::chrono::high_resolution_clock> g_first_token_time;
static bool g_is_first_token = true;
static unsigned long g_start_cpu_time = 0;

struct NpuUsage { float total_load; float avg_load; };

static void sig_exit(int sig) {
    if (g_llm_handle) {
        LLMHandle h = g_llm_handle;
        g_llm_handle = nullptr;
        rkllm_destroy(h);
    }
    exit(sig);
}

static long get_total_memory_mb() {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    return (pages > 0 && page_size > 0) ? (pages * page_size) / (1024 * 1024) : 0;
}

static long get_current_memory_mb() {
    std::ifstream f("/proc/self/statm");
    if (!f) return 0;
    long program_size, resident, share, text, lib, data, dt;
    f >> program_size >> resident >> share >> text >> lib >> data >> dt;
    long page_size_kb = sysconf(_SC_PAGESIZE) / 1024;
    return (resident * page_size_kb) / 1024;
}

static float get_npu_temperature() {
    const char* paths[] = { "/sys/class/thermal/thermal_zone0/temp", "/sys/class/thermal/thermal_zone1/temp" };
    for (const char* path : paths) {
        std::ifstream f(path);
        if (f) {
            float temp; f >> temp;
            if (f.good()) return temp / 1000.0f;
        }
    }
    return -1.0f;
}

static unsigned long get_process_cpu_time() {
    std::ifstream f("/proc/self/stat");
    if (!f) return 0;
    std::string pid, comm, state, ppid, pgrp, session, tty_nr, tpgid, flags;
    std::string minflt, cminflt, majflt, cmajflt;
    unsigned long utime, stime;
    f >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr >> tpgid >> flags
      >> minflt >> cminflt >> majflt >> cmajflt >> utime >> stime;
    return utime + stime;
}

static NpuUsage get_npu_usage() {
    NpuUsage u = {-1.0f, -1.0f};
    std::ifstream f("/sys/kernel/debug/rknpu/load");
    if (!f) return u;
    std::string line; std::getline(f, line);
    float total = 0; int cores = 0;
    size_t pos = 0;
    while ((pos = line.find("Core", pos)) != std::string::npos) {
        size_t pct = line.find('%', pos);
        if (pct != std::string::npos) {
            size_t col = line.rfind(':', pct);
            if (col != std::string::npos && col > pos) {
                try {
                    total += std::stof(line.substr(col + 1, pct - col - 1));
                    cores++;
                } catch (...) {}
            }
        }
        pos++;
    }
    if (cores > 0) { u.total_load = total; u.avg_load = total / cores; }
    return u;
}

static void print_llm_metrics() {
    auto end_time = std::chrono::high_resolution_clock::now();
    unsigned long end_cpu = get_process_cpu_time();
    double total_time = std::chrono::duration<double>(end_time - g_start_time).count();

    double cpu_total = 0, cpu_avg = 0;
    int n_cores = sysconf(_SC_NPROCESSORS_ONLN);
    if (total_time > 0 && n_cores > 0) {
        long ticks = sysconf(_SC_CLK_TCK);
        if (ticks > 0) {
            double cpu_sec = (double)(end_cpu - g_start_cpu_time) / ticks;
            cpu_total = 100.0 * cpu_sec / total_time;
            cpu_avg = cpu_total / n_cores;
        }
    }

    double ttft = std::chrono::duration<double>(g_first_token_time - g_start_time).count();
    double decode_time = total_time - ttft;
    double decode_speed = (g_token_count > 1 && decode_time > 0) ? (g_token_count - 1) / decode_time : 0.0;

    long cur_mem = get_current_memory_mb();
    long tot_mem = get_total_memory_mb();
    float mem_pct = (tot_mem > 0) ? 100.0f * cur_mem / tot_mem : 0;

    printf("\n----------------------------------\n");
    printf("Performance Metrics:\n");
    printf(" [Time] Total Cost   : %.4f s\n", total_time);
    printf(" [Time] First Token  : %.4f s (Latency)\n", ttft);
    printf(" [Count] Token Count : %d\n", g_token_count);
    printf(" [Speed] E2E Speed   : %.2f tokens/s\n", total_time > 0 ? g_token_count / total_time : 0.0);
    printf(" [Speed] Decode Speed: %.2f tokens/s\n", decode_speed);
    printf(" [HardW] Memory Usage : %ld MB / %ld MB (%.1f%%)\n", cur_mem, tot_mem, mem_pct);
    printf(" [HardW] CPU Util Total: %.2f %%\n", cpu_total);
    printf(" [HardW] CPU Util Avg  : %.2f %%\n", cpu_avg);
    NpuUsage npu = get_npu_usage();
    if (npu.total_load >= 0) {
        printf(" [HardW] NPU Util Total: %.2f %%\n", npu.total_load);
        printf(" [HardW] NPU Util Avg  : %.2f %%\n", npu.avg_load);
    }
    float temp = get_npu_temperature();
    if (temp > 0) printf(" [HardW] NPU Temp      : %.1f °C\n", temp);
    printf("----------------------------------\n");
}

static int llm_callback(RKLLMResult *result, void *, LLMCallState state) {
    if (state == RKLLM_RUN_FINISH) {
        g_llm_running = false;
        print_llm_metrics();
    } else if (state == RKLLM_RUN_NORMAL) {
        if (g_is_first_token) {
            g_first_token_time = std::chrono::high_resolution_clock::now();
            g_is_first_token = false;
        }
        g_token_count++;
        printf("%s", result->text);
        fflush(stdout);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <yolo.rknn> <llm.rkllm> [camera_id_or_video] [max_new_tokens] [max_context_len]\n", argv[0]);
        fprintf(stderr, "  camera: 0 or /dev/video0 = 默认; USB 摄像头通常为 /dev/video21\n");
        return 1;
    }
    signal(SIGINT, sig_exit);

    const char *yolo_model = argv[1];
    const char *llm_model = argv[2];
    /* 第 3 参数：摄像头。USB 摄像头在本板一般为 /dev/video21，内置或 0 为 0 */
    const char *capture_arg = argc > 3 ? argv[3] : "0";
    int max_new_tokens = argc > 4 ? atoi(argv[4]) : 256;
    int max_context_len = argc > 5 ? atoi(argv[5]) : 4096;

    // 打开摄像头或视频（优先 V4L2 以兼容 USB 摄像头）
    cv::VideoCapture cap;
    if (strstr(capture_arg, "/dev/video") != nullptr) {
        if (!cap.open(capture_arg, cv::CAP_V4L2))
            cap.open(capture_arg, cv::CAP_ANY);
    } else if (strlen(capture_arg) == 1 && capture_arg[0] >= '0' && capture_arg[0] <= '9') {
        cap.open(capture_arg[0] - '0');
    } else {
        cap.open(capture_arg);
    }
    if (!cap.isOpened()) {
        fprintf(stderr, "Failed to open capture: %s (USB camera try: /dev/video21)\n", capture_arg);
        return 1;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
    printf("Camera opened: %s\n", capture_arg);

    // 初始化 LLM
    RKLLMParam llm_param = rkllm_createDefaultParam();
    llm_param.model_path = llm_model;
    llm_param.max_new_tokens = max_new_tokens;
    llm_param.max_context_len = max_context_len;
    llm_param.top_k = 1;
    llm_param.top_p = 0.95f;
    llm_param.temperature = 0.8f;
    llm_param.repeat_penalty = 1.1f;
    llm_param.skip_special_token = 1;
    int ret = rkllm_init(&g_llm_handle, &llm_param, llm_callback);
    if (ret != 0) {
        fprintf(stderr, "rkllm_init failed: %d\n", ret);
        return 1;
    }

    static const bool use_gui = (getenv("DISPLAY") != nullptr);
    const int wait_ms = 30;
    char yolo_path[1024];
    snprintf(yolo_path, sizeof(yolo_path), "%s", yolo_model);

    if (use_gui) {
        // ========== GUI：YOLO 线程池（与单独 yolo demo 一致，提高 FPS 与 NPU 利用率）==========
        int n = INFERENCE_THREAD_NUM;
        int threads_per_core = (n + NpuCoreScheduler::NUM_NPU_CORES - 1) / NpuCoreScheduler::NUM_NPU_CORES;
        NpuCoreScheduler scheduler(threads_per_core);
        std::vector<rknn_lite*> rkpool;
        std::vector<std::chrono::steady_clock::time_point> slot_capture_time(n);
        std::vector<int> slot_to_frame(n);
        int next_frame_index = 0, display_next = 0;
        const size_t kCameraQueueMax = 2;
        std::mutex read_mutex;
        std::condition_variable read_cv;
        std::atomic<bool> read_eof{false};
        std::atomic<bool> read_reader_running{true};
        using steady_tp = std::chrono::steady_clock::time_point;
        std::queue<std::pair<cv::Mat, steady_tp>> camera_frame_queue;
        std::queue<cv::Mat> camera_buffer_pool;
        struct CompletedFrame { cv::Mat img; steady_tp capture_time; detect_result_group_t det; };
        std::map<int, CompletedFrame> completed_frames;
        std::deque<steady_tp> fps_window;
        std::deque<double> latency_window;
        const size_t kFpsWindowSize = 60;
        bool quit_requested = false;
        int in_flight = 0;

        for (int i = 0; i < n; i++) {
            rknn_lite *ptr = new rknn_lite(yolo_path, i % 3);
            rkpool.push_back(ptr);
            if (!cap.read(ptr->ori_img) || ptr->ori_img.empty()) {
                fprintf(stderr, "Failed to read initial frame %d\n", i);
                for (auto *p : rkpool) delete p;
                rkllm_destroy(g_llm_handle);
                g_llm_handle = nullptr;
                return 1;
            }
            slot_capture_time[i] = std::chrono::steady_clock::now();
            slot_to_frame[i] = next_frame_index++;
            scheduler.submit(i, [&rkpool, i]() { return rkpool[i]->interf(); });
            in_flight++;
        }
        for (size_t i = 0; i < kCameraQueueMax; i++) camera_buffer_pool.push(cv::Mat());
        std::thread read_reader_thread([&]() {
            while (read_reader_running && cap.isOpened()) {
                cv::Mat buf;
                {
                    std::unique_lock<std::mutex> l(read_mutex);
                    read_cv.wait(l, [&]() { return !camera_buffer_pool.empty() || read_eof; });
                    if (read_eof) break;
                    buf = std::move(camera_buffer_pool.front());
                    camera_buffer_pool.pop();
                }
                if (!cap.read(buf) || buf.empty()) {
                    read_eof = true;
                    read_cv.notify_one();
                    break;
                }
                {
                    std::unique_lock<std::mutex> l(read_mutex);
                    read_cv.wait(l, [&]() { return camera_frame_queue.size() < kCameraQueueMax || read_eof; });
                    if (read_eof) break;
                    camera_frame_queue.push({std::move(buf), std::chrono::steady_clock::now()});
                    read_cv.notify_one();
                }
            }
        });

        printf("YOLO + LLM 摄像头联调（线程池 %d）：按 空格 问「摄像头看到了什么？」，按 q 退出。\n\n", n);

        auto last_show_time = std::chrono::steady_clock::now();
        int vision_log_frames = 0;
        double fps_smooth = 0.0;
        const double smooth_alpha = 0.12;

        while (!quit_requested && in_flight > 0) {
            int slot_id = scheduler.wait_completion();
            if (slot_id < 0) { in_flight--; break; }
            int frame_idx = slot_to_frame[slot_id];
            CompletedFrame cf;
            cf.img = rkpool[slot_id]->ori_img.clone();
            cf.capture_time = slot_capture_time[slot_id];
            cf.det = *rkpool[slot_id]->get_last_result();
            completed_frames[frame_idx] = std::move(cf);
            {
                std::unique_lock<std::mutex> l(read_mutex);
                read_cv.wait(l, [&]() { return !camera_frame_queue.empty() || read_eof; });
                if (read_eof) { in_flight--; break; }
                auto &fr = camera_frame_queue.front();
                std::swap(rkpool[slot_id]->ori_img, fr.first);
                slot_capture_time[slot_id] = fr.second;
                slot_to_frame[slot_id] = next_frame_index++;
                camera_buffer_pool.push(std::move(fr.first));
                camera_frame_queue.pop();
                read_cv.notify_one();
            }
            scheduler.submit(slot_id, [&rkpool, slot_id]() { return rkpool[slot_id]->interf(); });

            while (completed_frames.count(display_next)) {
                auto it = completed_frames.find(display_next);
                double latency_ms = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - it->second.capture_time).count();
                fps_window.push_back(std::chrono::steady_clock::now());
                if (fps_window.size() > kFpsWindowSize) fps_window.pop_front();
                latency_window.push_back(latency_ms);
                if (latency_window.size() > kFpsWindowSize) latency_window.pop_front();
                double fps_display = 0.0;
                if (fps_window.size() >= 2) {
                    double el = std::chrono::duration<double, std::milli>(fps_window.back() - fps_window.front()).count();
                    if (el > 0.1) fps_display = (fps_window.size() - 1) * 1000.0 / el;
                }
                double avg_lat = 0.0;
                for (double v : latency_window) avg_lat += v;
                if (!latency_window.empty()) avg_lat /= latency_window.size();
                if (fps_smooth <= 0) fps_smooth = fps_display; else fps_smooth = smooth_alpha * fps_display + (1.0 - smooth_alpha) * fps_smooth;

                cv::Mat to_show = it->second.img;
                const detect_result_group_t *det = &it->second.det;
                cv::putText(to_show, "SPACE: ask what you see | Q: quit", cv::Point(10, 30),
                            cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
                char buf[80];
                snprintf(buf, sizeof(buf), "FPS: %.1f  Latency: %.0f ms", fps_smooth, avg_lat);
                cv::putText(to_show, buf, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);
                std::string vis_line = "objs: ";
                if (det->count > 0) {
                    vis_line += std::to_string(det->count) + "  ";
                    for (int i = 0; i < det->count && i < 8; i++) {
                        if (i > 0) vis_line += ", ";
                        vis_line += det->results[i].name;
                    }
                    if (det->count > 8) vis_line += "...";
                } else vis_line += "0";
                cv::putText(to_show, vis_line, cv::Point(10, 90), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(200, 200, 0), 2);
                cv::imshow("YOLO", to_show);

                vision_log_frames++;
                if (vision_log_frames >= (int)(fps_smooth > 0 ? fps_smooth : 15)) {
                    vision_log_frames = 0;
                    if (det->count > 0) {
                        std::string ns;
                        for (int i = 0; i < det->count; i++) { if (i) ns += ", "; ns += det->results[i].name; }
                        printf("[Vision] %d objs [%s]  FPS: %.1f  Latency: %.0f ms\n", det->count, ns.c_str(), fps_smooth, avg_lat);
                    } else printf("[Vision] 0 objs  FPS: %.1f  Latency: %.0f ms\n", fps_smooth, avg_lat);
                    fflush(stdout);
                }

                int key = cv::waitKey(wait_ms);
                if (key == 'q' || key == 'Q') { quit_requested = true; break; }
                if (key == 32 || key == 13) {
                    std::string names_str;
                    if (det->count > 0) {
                        for (int i = 0; i < det->count; i++) {
                            if (i > 0) names_str += "、";
                            names_str += det->results[i].name;
                        }
                    } else names_str = "（未检测到物体）";
                    printf("\n[当前画面] 检测到: %s\n", names_str.c_str());
                    printf("assistant: ");
                    fflush(stdout);
                    std::string prompt = "当前画面检测到以下物体：" + names_str + "。\n\n用户问：摄像头看到了什么？\n\n请根据画面内容简要回答：";
                    RKLLMInput llm_input;
                    memset(&llm_input, 0, sizeof(llm_input));
                    llm_input.input_type = RKLLM_INPUT_PROMPT;
                    llm_input.prompt_input = (char *)prompt.c_str();
                    RKLLMInferParam infer_param;
                    memset(&infer_param, 0, sizeof(infer_param));
                    infer_param.mode = RKLLM_INFER_GENERATE;
                    g_token_count = 0;
                    g_is_first_token = true;
                    g_start_time = std::chrono::high_resolution_clock::now();
                    g_start_cpu_time = get_process_cpu_time();
                    g_llm_running = true;
                    rkllm_run(g_llm_handle, &llm_input, &infer_param, nullptr);
                    while (g_llm_running) { usleep(50000); }
                    printf("\n\n");
                }
                completed_frames.erase(it);
                display_next++;
            }
        }
        read_reader_running = false;
        read_cv.notify_all();
        if (read_reader_thread.joinable()) read_reader_thread.join();
        for (auto *p : rkpool) delete p;
    } else {
        // ========== 无 GUI：单实例 YOLO，每帧询问 ==========
        rknn_lite *yolo = new rknn_lite(yolo_path, 0);
        printf("YOLO + LLM 摄像头联调（无 GUI）：每帧后输入问题，直接回车问「摄像头看到了什么？」。\n\n");
        cv::Mat frame;
        const detect_result_group_t *last_det = nullptr;
        while (true) {
            if (!cap.read(frame) || frame.empty()) { fprintf(stderr, "Read frame failed.\n"); break; }
            yolo->ori_img = frame;
            if (yolo->interf() != 0) continue;
            last_det = yolo->get_last_result();
            printf("\n[当前画面] 检测到: ");
            if (last_det && last_det->count > 0) {
                for (int i = 0; i < last_det->count; i++) { if (i > 0) printf("、"); printf("%s", last_det->results[i].name); }
            } else printf("（未检测到物体）");
            printf("\n你的问题（直接回车则问「摄像头看到了什么？」）: ");
            fflush(stdout);
            std::string user_question;
            if (!std::getline(std::cin, user_question)) break;
            if (user_question == "exit") break;
            if (user_question.empty()) user_question = "摄像头看到了什么？";
            std::string names_str;
            if (last_det && last_det->count > 0) {
                for (int i = 0; i < last_det->count; i++) { if (i > 0) names_str += "、"; names_str += last_det->results[i].name; }
            } else names_str = "（未检测到物体）";
            std::string prompt = "当前画面检测到以下物体：" + names_str + "。\n\n用户问：" + user_question + "\n\n请根据画面内容简要回答：";
            RKLLMInput llm_input;
            memset(&llm_input, 0, sizeof(llm_input));
            llm_input.input_type = RKLLM_INPUT_PROMPT;
            llm_input.prompt_input = (char *)prompt.c_str();
            RKLLMInferParam infer_param;
            memset(&infer_param, 0, sizeof(infer_param));
            infer_param.mode = RKLLM_INFER_GENERATE;
            g_token_count = 0;
            g_is_first_token = true;
            g_start_time = std::chrono::high_resolution_clock::now();
            g_start_cpu_time = get_process_cpu_time();
            g_llm_running = true;
            printf("assistant: ");
            fflush(stdout);
            rkllm_run(g_llm_handle, &llm_input, &infer_param, nullptr);
            while (g_llm_running) { usleep(50000); }
            printf("\n\n");
        }
        delete yolo;
    }

    cv::destroyAllWindows();
    rkllm_destroy(g_llm_handle);
    g_llm_handle = nullptr;
    return 0;
}
