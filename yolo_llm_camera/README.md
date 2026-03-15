# YOLO + LLM Camera Integration

中文 / Chinese: [README_CN.md](README_CN.md)

Ask the LLM "what do you see?", and it responds based on the current YOLO detection results. **Powered by the same acceleration as the standalone YOLO demo**: Multi-instance thread pool + 3 NPU cores running in parallel. Real-time display of FPS, latency, and visual data on screen and terminal. Full Performance Metrics are printed after each LLM response.

## Feature Overview

- **YOLO Thread Pool**: Default 6 inference instances (`INFERENCE_THREAD_NUM`, can be changed to 12 like the standalone YOLO), bound to 3 NPU cores in a round-robin fashion. Frame reading and inference run in parallel to boost FPS and NPU utilization.
- **On-Screen Display**: Top-left shows three lines: Operation prompts; **FPS**, **Latency (ms)**; **objs: N class1, class2, ...** (number of detections and class names).
- **Terminal Output**: About one line per second: `[Vision] N objs [class1, class2, ...] FPS: xx.x Latency: xx ms`.
- **Space/Enter Key**: Ask "What do you see?" using the detection results of the current frame directly, no need to type the question. After the LLM finishes responding, **Performance Metrics** (Total Cost, First Token, Token Count, E2E/Decode Speed, Memory, CPU/NPU, NPU Temp) are printed, consistent with the standalone LLM demo.
- **q Key**: Quit.

## Dependencies

- `yolo-rk3588/rknn-cpp-Multithreading` in this repository (YOLO model, 3rdparty, postprocess, NpuCoreScheduler).
- RKLLM Runtime (refer to the environment setup in `SDK_Chg`).
- `RKLLM_API_PATH` in CMake must point to the `librkllm_api` directory of your local rkllm-runtime.

## Build

```bash
cd yolo_llm_camera
./build-linux.sh
```

Artifacts are in `install/demo_Linux_aarch64/`: `yolo_llm_camera_demo` along with `lib/` and `model/` (if present).

## Run (On Board)

Push the following to the same directory on the board (e.g., `/home/admin/RKLLM/yolo_llm/`):

- The entire `install/demo_Linux_aarch64/` directory (or push its contents to `yolo_llm/demo_Linux_aarch64/`).
- The LLM model (e.g., `Qwen3-1.7B_W8A8_RK3588.rkllm`) placed alongside `demo_Linux_aarch64` (e.g., in `yolo_llm/`), so `../Qwen3-1.7B_W8A8_RK3588.rkllm` can access it.
- Ensure `model/RK3588/yolo26n.rknn` and `model/coco_80_labels_list.txt` are in the run directory (they are copied from the yolo project during installation if present).

**Enter the directory and execute:**

```bash
cd /home/admin/RKLLM/yolo_llm/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./yolo_llm_camera_demo model/RK3588/yolo26n.rknn ../Qwen3-1.7B_W8A8_RK3588.rkllm /dev/video21 256 4096
```

- **3rd Parameter**: Camera device (`0`, `/dev/video0` or commonly `/dev/video21` for USB) or video file path.
- **4th/5th Parameters**: `max_new_tokens`, `max_context_len` (optional, default 256, 4096).

Without DISPLAY (e.g., SSH without X11): No window will pop up. Instead, input your question in the terminal after each frame (pressing Enter directly asks "What do you see?").

## Thread Count (Optional)

In the source code, `INFERENCE_THREAD_NUM` defaults to 6. To use 12, matching the standalone YOLO demo, modify the top of `src/yolo_llm_camera_demo.cpp`:

```cpp
#define INFERENCE_THREAD_NUM  12
```

Recompile for the change to take effect.

## Docs Index

- Overall repository & deployment: Root `README.md`, `SDK_Chg/00.Linux_image_preset_NPU_environment-implementation_plan.md`.
- Standalone YOLO multi-threading & NPU scheduling: `yolo-rk3588/rknn-cpp-Multithreading/README.md`.