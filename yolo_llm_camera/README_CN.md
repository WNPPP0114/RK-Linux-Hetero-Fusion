# YOLO + LLM 摄像头联调

中文 / Chinese: [README_CN.md](README_CN.md)

你问 LLM「摄像头看到了什么」，LLM 根据当前画面的 YOLO 检测结果回答。**与单独 YOLO demo 同款加速**：多实例线程池 + 3 核 NPU 并行，画面与终端实时显示 FPS、延迟与视觉数据，每次 LLM 回答后打印完整 Performance Metrics。

## 功能概览

- **YOLO 线程池**：默认 6 个推理实例（`INFERENCE_THREAD_NUM`，可与单独 yolo 一致改为 12），轮询绑定 3 个 NPU 核，读帧与推理并行，提升 FPS 与 NPU 利用率。
- **画面显示**：左上角三行——操作提示；**FPS**、**Latency (ms)**；**objs: N  class1, class2, ...**（检测数量与类别名）。
- **终端打印**：约每秒一行 `[Vision] N objs [class1, class2, ...]  FPS: xx.x  Latency: xx ms`。
- **按空格/回车**：用当前帧检测结果直接问「摄像头看到了什么？」，无需再输入问题；LLM 回答结束后打印 **Performance Metrics**（Total Cost、First Token、Token Count、E2E/Decode Speed、Memory、CPU/NPU、NPU Temp），与纯 LLM demo 一致。
- **按 q**：退出。

## 依赖

- 本仓库内 `yolo-rk3588/rknn-cpp-Multithreading`（YOLO 模型、3rdparty、postprocess、NpuCoreScheduler）
- RKLLM 运行时（参考 `SDK_Chg` 中的环境搭建）
- CMake 中 `RKLLM_API_PATH` 需指向本机 rkllm-runtime 的 `librkllm_api` 目录

## 编译

```bash
cd yolo_llm_camera
./build-linux.sh
```

产物在 `install/demo_Linux_aarch64/`：`yolo_llm_camera_demo` 与 `lib/`、`model/`（若存在）。

## 运行（开发板）

将以下内容推到板子同一目录（例如 `/home/admin/RKLLM/yolo_llm/`）：

- `install/demo_Linux_aarch64/` 整个目录（或将其内容推到 `yolo_llm/demo_Linux_aarch64/`）
- LLM 模型（如 `Qwen3-1.7B_W8A8_RK3588.rkllm`）放在与 `demo_Linux_aarch64` 同级（如 `yolo_llm/`），以便 `../Qwen3-1.7B_W8A8_RK3588.rkllm` 能访问
- 确保 `model/RK3588/yolo26n.rknn` 与 `model/coco_80_labels_list.txt` 在运行目录下（安装时若存在会从 yolo 工程拷贝）

**进入目录并执行：**

```bash
cd /home/admin/RKLLM/yolo_llm/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./yolo_llm_camera_demo model/RK3588/yolo26n.rknn ../Qwen3-1.7B_W8A8_RK3588.rkllm /dev/video21 256 4096
```

- **第 3 参数**：摄像头设备（`0`、`/dev/video0` 或 USB 常用 `/dev/video21`）或视频文件路径。
- **第 4/5 参数**：`max_new_tokens`、`max_context_len`（可省略，默认 256、4096）。

无 DISPLAY 时（如 SSH 无 X11）：不弹窗，改为每帧后在终端输入问题（直接回车即问「摄像头看到了什么？」）。

## 线程数（可选）

源码中默认 `INFERENCE_THREAD_NUM` 为 6。若要与单独 yolo demo 一致用 12，可修改 `src/yolo_llm_camera_demo.cpp` 顶部：

```cpp
#define INFERENCE_THREAD_NUM  12
```

重新编译后生效。

## 文档索引

- 整体仓库与部署：仓库根目录 `README.md`、`SDK_Chg/00.Linux_image_preset_NPU_environment-implementation_plan.md`。
- 单独 YOLO 多线程与 NPU 调度：`yolo-rk3588/rknn-cpp-Multithreading/README.md`。
