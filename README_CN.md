# RK-Linux-Hetero-Fusion

**C++ · aarch64 · RKNN · RKLLM · YOLO · Qwen3**

English: [README.md](README.md)

---

本仓库为 **RK3588 / RK3588S** 端侧部署与联调项目：包含纯视觉 YOLO 的 C++ 部署、**YOLO + LLM 多模态**（摄像头问「看到了什么」、Token 直通注入）、以及基于 Ubuntu 的 SDK 环境定制编译。所有 C++ demo 面向 aarch64 交叉编译，在板端运行。

### 仓库结构

| 目录 | 说明 |
|------|------|
| **yolo-rk3588/** | YOLO 在 RK3588 上的 C++ 推理（多线程、RGA+NPU、DRM Zero-Copy），当前预置 `yolo26n.rknn` 和 `yolo26s.rknn` 模型，源码需作为子模块拉取。 |
| **yolo_llm_camera/** | **YOLO + LLM 摄像头联调**：与单独 YOLO 同款线程池（默认 6 实例、3 核 NPU 并行），画面显示 FPS / 延迟 / 检测数量与类别；按空格问「摄像头看到了什么？」，回答后打印 Performance Metrics。 |
| **multimodal-token-injection/** | **语义 Token 直通**：YOLO 类别 ID → Qwen 词表 ID 映射，`RKLLM_INPUT_TOKEN` 输入，跳过 Tokenizer/JSON，目标约 14 Tokens/s。 |
| **SDK_Chg/** | **环境搭建与系统定制**：包含 RKNN-Toolkit2 / RKLLM 环境搭建指南，以及将 NPU 环境与依赖预置入 Ubuntu rootfs 镜像的实施计划（实现开机即用并去 topeet 化为 admin 用户）。 |
| **ubuntu20.04_docker/** | **Docker 编译环境**：提供 Ubuntu 20.04 容器的配置指令，用于稳定地编译上述修改后的定制化 SDK 与镜像。 |

### 快速参考：进入目录与执行指令

路径以**本仓库根目录**为基准；开发板路径以 `/home/admin/RKLLM/` 为例（假设通过 `SDK_Chg` 已将默认用户修改为 admin），可按实际修改。

#### 一、纯 YOLO（yolo-rk3588）

*注：确保你已获取 `rknn-cpp-Multithreading` 源码。*

```bash
# 编译
cd yolo-rk3588/rknn-cpp-Multithreading
./build-linux_RK3588.sh

# 板端运行（进入安装目录后）
LD_LIBRARY_PATH=./lib ./rknn_yolo_demo model/RK3588/yolo26n.rknn /dev/video21
```

#### 二、YOLO + LLM 摄像头联调（yolo_llm_camera）

```bash
# 编译
cd yolo_llm_camera
./build-linux.sh

# 推送后，板端运行（USB 摄像头一般为 /dev/video21）
cd /home/admin/RKLLM/yolo_llm/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./yolo_llm_camera_demo model/RK3588/yolo26n.rknn ../Qwen3-1.7B_W8A8_RK3588.rkllm /dev/video21 256 4096
```

- 窗口左上角：**FPS**、**Latency (ms)**、**objs + 类别名**
- 终端：约每秒 `[Vision] N objs [...] FPS Latency`
- **空格**：问「摄像头看到了什么？」；每次回答后打印 **Performance Metrics**
- **q**：退出

详见 [yolo_llm_camera/README_CN.md](yolo_llm_camera/README_CN.md)。

#### 三、多模态 Token 注入（multimodal-token-injection）

```bash
# 编译
cd multimodal-token-injection
./build-linux.sh

# 板端运行（可选末尾加类别 ID，如 0 2 15 16）
cd /home/admin/RKLLM/token_injection/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./multimodal_token_injection_demo ../Qwen3-1.7B_W8A8_RK3588.rkllm 256 4096
```

详见 [multimodal-token-injection/README_CN.md](multimodal-token-injection/README_CN.md)。

### 依赖与环境

- **板子**：RK3588/RK3588S，带 RKNN/RKLLM 驱动与运行时
- **交叉编译**：aarch64 gcc、CMake；YOLO 需 OpenCV、RGA、RKNN；LLM 需 RKLLM（`librkllm_api`）
- **定制系统编译**：详见 `SDK_Chg` 与 `ubuntu20.04_docker` 目录。
- **模型**：需自备 YOLO 的 `.rknn` 以及基于 Qwen 转换的 `.rkllm`。

### 文档索引

| 文档 | 说明 |
|------|------|
| [SDK_Chg/00.Linux_image_preset_NPU_environment-implementation_plan.md](SDK_Chg/00.Linux_image_preset_NPU_environment-implementation_plan.md) | 环境预置与用户系统修改 |
| [SDK_Chg/rknn-toolkit2_environment_setup.txt](SDK_Chg/rknn-toolkit2_environment_setup.txt) | RKNN-Toolkit2 环境搭建指南 |
| [SDK_Chg/rknn-llm_environment_setup.txt](SDK_Chg/rknn-llm_environment_setup.txt) | RKLLM 部署参考 |
| [yolo_llm_camera/README_CN.md](yolo_llm_camera/README_CN.md) | YOLO+LLM 摄像头联调 |
| [multimodal-token-injection/README_CN.md](multimodal-token-injection/README_CN.md) | Token 直通多模态 |
| [ubuntu20.04_docker/01.Docker_installation_instructions.txt](ubuntu20.04_docker/01.Docker_installation_instructions.txt) | Docker 安装指南 |
| [ubuntu20.04_docker/02.Docker_configure_ubuntu20.04_environment.txt](ubuntu20.04_docker/02.Docker_configure_ubuntu20.04_environment.txt) | Docker 编译环境配置 |
