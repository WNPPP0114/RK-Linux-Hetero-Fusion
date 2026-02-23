[简体中文](./README_CN.md) | [English](./README.md)

# RK-Linux-Hetero-Fusion

**Embedded Linux Heterogeneous Computing Framework: Decoupled Vision & Reasoning on RK3588**

[![Platform](https://img.shields.io/badge/Platform-Rockchip%20RK3588-blue?logo=linux&logoColor=white)](https://www.rock-chips.com/)
[![OS](https://img.shields.io/badge/OS-Embedded%20Linux%20(5.10%20LTS)-green?logo=tux&logoColor=white)](https://kernel.org)
[![Framework](https://img.shields.io/badge/Framework-RKNN%20%7C%20MPP%20%7C%20RGA%20%7C%20Qt6-orange)](https://github.com/airockchip/rknn-toolkit2)
[![License](https://img.shields.io/badge/License-Apache%202.0-lightgrey)](LICENSE)

## 📖 Introduction

**RK-Linux-Hetero-Fusion** is a high-performance, heterogeneous edge AI framework engineered specifically for the Rockchip RK3588 platform. It transcends traditional "vision-only" edge systems by fusing **Real-time Object Detection (YOLO)** with **Semantic Reasoning (DeepSeek LLM)** into a unified, zero-copy pipeline.

Current edge AI solutions often suffer from memory bottlenecks and serialized processing. RK-Linux-Hetero-Fusion solves this by implementing a **DMA-BUF (DRM) Zero-Copy architecture**, allowing the CPU, NPU, RGA, and GPU to share memory without redundant copying. The result is a system capable of **60+ FPS visual perception**, **14 tokens/s complex logic reasoning**, and **4K UI rendering** simultaneously, enabling true "Embedded AGI" capabilities on <12W power consumption.

> **Core Philosophy:**
> *   **Cost-Efficiency:** Running LLMs on Edge (RK3588) instead of Cloud GPUs.
> *   **Real-time Performance:** Zero-Copy pipelines for <16ms latency.
> *   **Robustness:** System stability under full NPU load via PREEMPT_RT.

## 🚀 Key Features

### 1. Optimized Linux BSP Construction
*   **Custom BSP Stack**: Built from Rockchip SDK with tailored U-Boot, Kernel (5.10 LTS), and minimal RootFS.
*   **Hardware-Specific DTS**: Precise device tree overlays for MIPI-CSI cameras, 3-core NPU partitioning, and PCIe accelerators.
*   **System Hardening**: Integrated **PREEMPT_RT** patch for deterministic latency; memory map optimization reduces boot time by 40%.

### 2. Zero-Copy Heterogeneous Pipeline
*   **MPP Hardware Acceleration**: 8-channel 1080p@30fps H.264/H.265 decoding with near-zero CPU usage.
*   **RGA Preprocessing**: Hardware-accelerated color space conversion (NV12→RGB) and resizing at 1200MPix/s throughput.
*   **DRM Zero-Copy Mechanism**: Direct memory mapping between VPU (Decoder), RGA (Pre-proc), and NPU (Inference), eliminating **~2.8GB/s** of redundant memory copy operations.

### 3. Dual-Path Intelligence Architecture
*   **Path A: Real-time Vision (YOLOv11)**
    *   Deploying state-of-the-art YOLOv11 on NPU with operator fusion and quantization calibration.
    *   Generates structured JSON perception data at **60+ FPS**.
*   **Path B: Semantic Reasoning (Qwen3-1.7B)**
    *   Fully offline LLM deployment via RKLLM with **W4A16 quantization**.
    *   Acts as the system's "Prefrontal Cortex," analyzing vision-generated JSON to make context-aware decisions.
    *   Achieves **14 tokens/s** generation speed through asynchronous pipeline optimizations.

### 4. Real-time HMI Dashboard (Qt6/QML)
*   **Zero-Copy Rendering**: Utilizes `EGL_LINUX_DMA_BUF_EXT` to map video buffers directly to GPU textures, bypassing CPU memory copy completely.
*   **Interactive "Thought Chain"**: Visualizes the LLM's reasoning process with a typewriter effect alongside the video stream.
*   **Performance Monitor**: Real-time plotting of NPU usage, temperature, and DDR bandwidth using custom QChart widgets.

## 🏗 Architecture

```mermaid
graph TD
    subgraph "Dual-Path Architecture"
        direction TB
        
        Cam[Camera Streams] -->|"Raw Video Frames"| V1
        
        subgraph "Fast Loop: Visual Perception (60 FPS)"
            V1["Stage 1: MPP Decoder<br/>8-ch 1080p@30fps"] 
            V2["Stage 2: RGA Preprocessor<br/>NV12→RGB Zero-Copy"]
            V3["Stage 3: YOLOv11 NPU Inference<br/>Object Detection"]
            V4["Stage 4: JSON Generator<br/>Structured Scene Graph"]
            
            V1 --> V2
            V2 --> V3
            V3 --> V4
        end
        
        V4 -.->|"Snapshot & JSON Data<br/>(Async Trigger)"| R1
        
        subgraph "Slow Loop: Cognitive Reasoning (14 tokens/s)"
            R1["Stage 1: Context Window<br/>Shared DRAM Buffer"]
            R2["Stage 2: Qwen3-1.7B<br/>W4A16 Quantized LLM"]
            R3["Stage 3: Decision Engine<br/>Logic & Safety Checks"]
            
            R1 --> R2
            R2 --> R3
        end
        
        subgraph "HMI Visualization Layer"
            UI1["GPU Texture Mapping<br/>(EGLImage)"]
            UI2["Thought Chain Display<br/>(QML Typewriter)"]
        end

        V2 -.->|"DMA-BUF FD"| UI1
        R3 --> UI2
    end

    %% --- Styling Definition ---
    style V1 fill:#D1E8FF,stroke:#0050B3,stroke-width:2px,color:#003A8C
    style V2 fill:#D1E8FF,stroke:#0050B3,stroke-width:2px,color:#003A8C
    style V4 fill:#D1E8FF,stroke:#0050B3,stroke-width:2px,color:#003A8C
    style R1 fill:#D9F7BE,stroke:#237804,stroke-width:2px,color:#135200
    style R3 fill:#D9F7BE,stroke:#237804,stroke-width:2px,color:#135200
    style V3 fill:#FFD8BF,stroke:#D4380D,stroke-width:3px,color:#871400
    style R2 fill:#FFD8BF,stroke:#D4380D,stroke-width:3px,color:#871400
    style UI1 fill:#EFDBFF,stroke:#391085,stroke-width:2px,color:#391085
    style UI2 fill:#EFDBFF,stroke:#391085,stroke-width:2px,color:#391085
    style Cam fill:#F5F5F5,stroke:#595959,stroke-width:2px,color:#262626
```

## 📊 Performance Benchmarks(2K 27-inch screen, HDMI)

| Component | Configuration | Throughput | Resource Usage | Efficiency Gain |
| :--- | :--- | :--- | :--- | :--- |
| **Vision Pipeline** | Baseline (Sequential) | 14 FPS | CPU: 78%, NPU: 45% | - |
| **Vision Pipeline** | **Hetero-Fusion (Async + Zero-Copy)** | **62 FPS** | **CPU: 35%**, NPU: 98% | **2.0× Speed, 0.5× CPU** |
| **LLM Inference** | Single-thread | 6 tokens/s | NPU: 60% load | - |
| **LLM Inference** | **Hetero-Fusion (3-Core Parallel)** | **14 tokens/s** | NPU: 99% load | **2.3× Speed** |
| **Rendering** | CPU Qt Paint | 15 FPS | CPU: 65% | - |
| **Rendering** | **EGL DMA-BUF** | **60 FPS** | **CPU: 2%**, GPU: 15% | **Smooth UI** |

## 🛠️ Build & Usage

### Prerequisites
*   **Hardware**: Rockchip RK3588/RK3588S (8GB+ RAM required for LLM)
*   **Host**: Ubuntu 20.04 LTS (Docker required)
*   **SDK**: Rockchip Linux SDK 5.10 + RKNN Toolkit2 v2.0+

### 1. System Setup (Docker)
We provide a pre-configured Docker image containing the Cross-Compiler, CMake, and RKNN dependencies.

```bash
# Clone repository
git clone --recursive https://github.com/WNPPP0114/RK-Linux-Hetero-Fusion.git
cd RK-Linux-Hetero-Fusion

# Initialize build environment
docker build -t fusion-builder -f docker/Dockerfile .
docker run -v $(pwd):/workspace -it fusion-builder
```

### 2. Build BSP & Firmware
```bash
# Build custom U-Boot, Kernel, and RootFS
cd bsp
./configure --platform=rk3588 --board=itop-3588
./build.sh all
```

### 3. Compile Core Application
```bash
cd src
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain/rk3588_linux.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_GUI=ON ..
make -j$(nproc)
```

### 4. Deploy & Run
```bash
# Transfer artifacts to board
scp ./fusion_core user@rk3588:/usr/local/bin/
scp -r ../models user@rk3588:/opt/fusion/

# Run the Fusion Daemon on device
export DISPLAY=:0
sudo fusion_core \
  --vision_model /opt/fusion/models/yolov11s.rknn \
  --llm_model /opt/fusion/models/qwen3-1.7b_w4a16.rknn \
  --enable_gl_render true
```

## 📂 Project Structure

```text
fusion/
├── 📂 bsp/                      # Board Support Package (Configs & Scripts)
│   ├── kernel_config            # Linux 5.10 custom defconfig
│   └── dts/                     # Device Tree Overlays
│
├── 📂 src/                      # Application Source Code
│   ├── 🔹 main.cpp              # Entry point
│   ├── 📂 core/                 # Thread Pools & Schedulers
│   ├── 📂 modules/              # YOLO / Qwen3 Logic
│   ├── 📂 ui/                   # Qt6/QML Dashboard
│   └── 📂 hal/                  # Hardware Abstraction (MPP/RGA/DRM)
│
├── 📂 third_party/              # Dependencies (librga, rknpu2)
├── 📂 models/                   # ONNX/RKNN Models
└── 📂 docker/                   # Build Environment
```

## 🛣️ Roadmap
- [x] **Phase 1**: Core pipeline with Zero-Copy (Done).
- [x] **Phase 2**: Qwen3 LLM integration (Done).
- [ ] **Phase 3**: **Qt HMI with EGL Image Integration** (In Progress).
- [ ] **Phase 4**: Multi-modal inputs (Audio/STT) for voice interaction.

## 🤝 Contribution
Contributions are welcome! Please see our [Contribution Guidelines](CONTRIBUTING.md).

---
**Maintainer**: WNPPP0114  
**Hardware Platform**: Rockchip RK3588 (8GB RAM)  
**GitHub**: https://github.com/WNPPP0114/RK-Linux-Hetero-Fusion
