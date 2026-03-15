# RK-Linux-Hetero-Fusion

**C++ · aarch64 · RKNN · RKLLM · YOLO · Qwen3**

中文 / Chinese: [README_CN.md](README_CN.md)

---

This repository provides **RK3588 / RK3588S** edge deployment and integration for vision and LLM tasks. It includes standalone YOLO C++ inference, **YOLO + LLM multimodal** integration (camera “what do you see?”, token injection), and guides for building a customized Ubuntu rootfs image with pre-installed NPU environments. All C++ demos are cross-compiled for aarch64 and run on-device.

### Repository structure

| Path | Description |
|------|-------------|
| **yolo-rk3588/** | YOLO C++ inference on RK3588 (multi-threaded, RGA+NPU, DRM zero-copy). Currently contains `.rknn` models, source code needs to be pulled as a submodule. |
| **yolo_llm_camera/** | **YOLO + LLM camera**: same thread pool as standalone YOLO (default 6 instances, 3 NPU cores). On-screen FPS, latency, detection count & classes; terminal `[Vision]` ~1/s; **Space** asks “What do you see?”; each reply prints Performance Metrics. |
| **multimodal-token-injection/** | **Semantic token passthrough**: YOLO class ID → Qwen vocab ID mapping, `RKLLM_INPUT_TOKEN` input, no tokenizer/JSON; target ~14 Tokens/s. |
| **SDK_Chg/** | **Environment setup & System customization**: Guides for setting up RKNN-Toolkit2/RKLLM, and a comprehensive plan to pre-install NPU environments and dependencies directly into the Ubuntu rootfs image (ready-to-use after flashing, changing default user from topeet to admin). |
| **ubuntu20.04_docker/** | **Docker build environment**: Configuration instructions for an Ubuntu 20.04 container, designed to stably compile the customized SDK and images mentioned above. |

### Quick reference: directory & commands

Paths are relative to the **repo root**; board paths use `/home/admin/RKLLM/` as an example (assuming the default user has been changed to `admin` via `SDK_Chg`).

#### 1. Standalone YOLO (yolo-rk3588)

*Note: Ensure you have fetched the `rknn-cpp-Multithreading` source code.*

```bash
# Build
cd yolo-rk3588/rknn-cpp-Multithreading
./build-linux_RK3588.sh

# On device (from install dir)
LD_LIBRARY_PATH=./lib ./rknn_yolo_demo model/RK3588/yolo26n.rknn /dev/video21
```

#### 2. YOLO + LLM camera (yolo_llm_camera)

```bash
# Build
cd yolo_llm_camera
./build-linux.sh

# On device (USB camera often /dev/video21)
cd /home/admin/RKLLM/yolo_llm/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./yolo_llm_camera_demo model/RK3588/yolo26n.rknn ../Qwen3-1.7B_W8A8_RK3588.rkllm /dev/video21 256 4096
```

- On-screen: **FPS**, **Latency (ms)**, **objs + class names**
- Terminal: `[Vision] N objs [...] FPS Latency` about once per second
- **Space**: ask “What do you see?”; **Performance Metrics** printed after each reply
- **q**: quit

See [yolo_llm_camera/README.md](yolo_llm_camera/README.md).

#### 3. Multimodal token injection (multimodal-token-injection)

```bash
# Build
cd multimodal-token-injection
./build-linux.sh

# On device (optional class IDs at end, e.g. 0 2 15 16)
cd /home/admin/RKLLM/token_injection/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./multimodal_token_injection_demo ../Qwen3-1.7B_W8A8_RK3588.rkllm 256 4096
```

See [multimodal-token-injection/README.md](multimodal-token-injection/README.md).

### Requirements

- **Board**: RK3588/RK3588S with RKNN/RKLLM drivers and runtime
- **Host**: aarch64 cross-compiler, CMake; YOLO needs OpenCV, RGA, RKNN; LLM needs RKLLM (`librkllm_api`)
- **Custom System Build**: See `SDK_Chg` and `ubuntu20.04_docker` directories.
- **Models**: You need to provide your own YOLO `.rknn` models and Qwen-based `.rkllm` models.

### Docs index

| Doc | Description |
|-----|-------------|
| [SDK_Chg/00.Linux_image_preset_NPU_environment-implementation_plan.md](SDK_Chg/00.Linux_image_preset_NPU_environment-implementation_plan.md) | Pre-install NPU env & User rename (Chinese) |
| [SDK_Chg/rknn-toolkit2_environment_setup.txt](SDK_Chg/rknn-toolkit2_environment_setup.txt) | RKNN-Toolkit2 deployment guide |
| [SDK_Chg/rknn-llm_environment_setup.txt](SDK_Chg/rknn-llm_environment_setup.txt) | RKLLM deployment guide |
| [yolo_llm_camera/README.md](yolo_llm_camera/README.md) | YOLO+LLM camera |
| [multimodal-token-injection/README.md](multimodal-token-injection/README.md) | Token passthrough |
| [ubuntu20.04_docker/01.Docker_installation_instructions.txt](ubuntu20.04_docker/01.Docker_installation_instructions.txt) | Docker installation guide |
| [ubuntu20.04_docker/02.Docker_configure_ubuntu20.04_environment.txt](ubuntu20.04_docker/02.Docker_configure_ubuntu20.04_environment.txt) | Docker build env configuration |
