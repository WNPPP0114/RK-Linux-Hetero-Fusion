# RK3588 Edge Vision & Multimodal LLM

**C++ · aarch64 · RKNN · RKLLM · YOLO · Qwen3**

中文 / Chinese: [README_CN.md](README_CN.md)

---

This repository provides **RK3588 / RK3588S** edge deployment and integration: standalone YOLO, standalone LLM (Qwen3-1.7B), and **YOLO + LLM multimodal** (camera “what do you see?”, token injection). All C++ demos are cross-compiled for aarch64 and run on-device.

### Repository structure

| Path | Description |
|------|-------------|
| **yolo-rk3588/** | YOLO C++ inference on RK3588 (multi-threaded, RGA+NPU, DRM zero-copy). Supports YOLOv5 / YOLO26·YOLOv8 / single-head 300×6. |
| **qwen3-1.7b/** | Qwen3-1.7B model export and C++ deploy; outputs `.rkllm` and `llm_demo`. |
| **yolo_llm_camera/** | **YOLO + LLM camera**: same thread pool as standalone YOLO (default 6 instances, 3 NPU cores). On-screen FPS, latency, detection count & classes; terminal `[Vision]` ~1/s; **Space** asks “What do you see?”; each reply prints Performance Metrics. |
| **multimodal-token-injection/** | **Semantic token passthrough**: YOLO class ID → Qwen vocab ID mapping, `RKLLM_INPUT_TOKEN` input, no tokenizer/JSON; target ~14 Tokens/s. |
| **02.rknn-llm部署.txt** | Full deployment steps (export, build, adb push, on-device run). |

### Quick reference: directory & commands

Paths are relative to the **repo root**; board paths use `/home/topeet/RKLLM/` as an example.

#### 1. Standalone YOLO (yolo-rk3588)

```bash
# Build
cd yolo-rk3588/rknn-cpp-Multithreading
./build-linux_RK3588.sh

# On device (from install dir)
LD_LIBRARY_PATH=./lib ./rknn_yolo_demo model/RK3588/yolo26n.rknn /dev/video21
```

See [yolo-rk3588/rknn-cpp-Multithreading/README.md](yolo-rk3588/rknn-cpp-Multithreading/README.md).

#### 2. Standalone LLM (qwen3-1.7b)

```bash
# Export
cd qwen3-1.7b/export
python3 generate_data_quant.py
python3 export_rkllm.py

# Build (set RKLLM_API_PATH in deploy/CMakeLists.txt)
cd qwen3-1.7b/deploy
./build-linux.sh

# On device
cd /home/topeet/RKLLM/qwen3-1.7b
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./llm_demo Qwen3-1.7B_W8A8_RK3588.rkllm 4096 4096
```

See `02.rknn-llm部署.txt` (Chinese deployment guide).

#### 3. YOLO + LLM camera (yolo_llm_camera)

```bash
# Build
cd yolo_llm_camera
./build-linux.sh

# On device (USB camera often /dev/video21)
cd /home/topeet/RKLLM/yolo_llm/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./yolo_llm_camera_demo model/RK3588/yolo26n.rknn ../Qwen3-1.7B_W8A8_RK3588.rkllm /dev/video21 256 4096
```

- On-screen: **FPS**, **Latency (ms)**, **objs + class names**
- Terminal: `[Vision] N objs [...] FPS Latency` about once per second
- **Space**: ask “What do you see?”; **Performance Metrics** printed after each reply
- **q**: quit

See [yolo_llm_camera/README.md](yolo_llm_camera/README.md) and `02.rknn-llm部署.txt` §4.

#### 4. Multimodal token injection (multimodal-token-injection)

```bash
# Generate token map (optional, needs transformers)
cd multimodal-token-injection
python3 scripts/generate_yolo_qwen_token_map.py --qwen-dir ../qwen3-1.7b/Qwen3-1.7B --output include/yolo_qwen_token_map.h

# Build
./build-linux.sh

# On device (optional class IDs at end, e.g. 0 2 15 16)
cd /home/topeet/RKLLM/qwen3-1.7b/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./multimodal_token_injection_demo ../Qwen3-1.7B_W8A8_RK3588.rkllm 256 4096
```

See [multimodal-token-injection/README.md](multimodal-token-injection/README.md).

### Merging multimodal-token-injection and yolo_llm_camera?

**Yes.** Two common approaches:

| Project | Role | LLM input |
|---------|------|-----------|
| **multimodal-token-injection** | LLM + token injection only (no camera/YOLO) | CLI class_id → token id → `RKLLM_INPUT_TOKEN` |
| **yolo_llm_camera** | Camera + YOLO + LLM | Detection names as text → `RKLLM_INPUT_PROMPT` |

- **Light merge (recommended)**: Add a `--token-injection` mode to yolo_llm_camera; keep multimodal-token-injection as a small standalone demo.
- **Full merge**: One project (yolo_llm_camera) with text prompt, token injection, and token-only (no camera) modes.

### Requirements

- **Board**: RK3588/RK3588S with RKNN/RKLLM drivers and runtime
- **Host**: aarch64 cross-compiler, CMake; YOLO needs OpenCV, RGA, RKNN; LLM needs RKLLM (`librkllm_api`)
- **Models**: YOLO `.rknn` and `model/coco_80_labels_list.txt`; Qwen3-1.7B `.rkllm`
- Details, adb push and troubleshooting: **`02.rknn-llm部署.txt`** (Chinese)

### Docs index

| Doc | Description |
|-----|-------------|
| [02.rknn-llm部署.txt](02.rknn-llm部署.txt) | Full deployment & adb (Chinese) |
| [yolo-rk3588/rknn-cpp-Multithreading/README.md](yolo-rk3588/rknn-cpp-Multithreading/README.md) | Standalone YOLO |
| [yolo_llm_camera/README.md](yolo_llm_camera/README.md) | YOLO+LLM camera |
| [multimodal-token-injection/README.md](multimodal-token-injection/README.md) | Token passthrough |
