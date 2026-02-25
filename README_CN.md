# RK-Linux-Hetero-Fusion

**C++ · aarch64 · RKNN · RKLLM · YOLO · Qwen3**

English: [README.md](README.md)

---

本仓库为 **RK3588 / RK3588S** 端侧部署与联调：纯视觉 YOLO、纯 LLM（Qwen3-1.7B）、以及 **YOLO + LLM 多模态**（摄像头问「看到了什么」、Token 直通注入）。所有 C++ demo 面向 aarch64 交叉编译，在板端运行。

### 仓库结构

| 目录 / 文件 | 说明 |
|-------------|------|
| **yolo-rk3588/** | YOLO 在 RK3588 上的 C++ 推理（多线程、RGA+NPU、DRM Zero-Copy），支持 YOLOv5 / YOLO26·YOLOv8 / 单输出 300×6。 |
| **qwen3-1.7b/** | Qwen3-1.7B 的模型转换（export）与 C++ 部署（deploy），产出 `.rkllm` 与 `llm_demo`。 |
| **yolo_llm_camera/** | **YOLO + LLM 摄像头联调**：与单独 YOLO 同款线程池（默认 6 实例、3 核 NPU 并行），画面显示 FPS / 延迟 / 检测数量与类别；终端约每秒打印 `[Vision]`；按空格问「摄像头看到了什么？」，每次回答后打印 Performance Metrics。 |
| **multimodal-token-injection/** | **语义 Token 直通**：YOLO 类别 ID → Qwen 词表 ID 映射，`RKLLM_INPUT_TOKEN` 输入，跳过 Tokenizer/JSON，目标约 14 Tokens/s。 |
| **02.rknn-llm部署.txt** | 模型转换、编译、adb 推送、板端运行等**完整部署步骤**。 |

### 快速参考：进入目录与执行指令

路径以**本仓库根目录**为基准；开发板路径以 `/home/topeet/RKLLM/` 为例，可按实际修改。

#### 一、纯 YOLO（yolo-rk3588）

```bash
# 编译
cd yolo-rk3588/rknn-cpp-Multithreading
./build-linux_RK3588.sh

# 板端运行（进入安装目录后）
LD_LIBRARY_PATH=./lib ./rknn_yolo_demo model/RK3588/yolo26n.rknn /dev/video21
```

详见 [yolo-rk3588/rknn-cpp-Multithreading/README.md](yolo-rk3588/rknn-cpp-Multithreading/README.md)。

#### 二、纯 LLM（qwen3-1.7b）

```bash
# 模型转换
cd qwen3-1.7b/export
python3 generate_data_quant.py
python3 export_rkllm.py

# 编译（需在 deploy/CMakeLists.txt 中设置 RKLLM_API_PATH）
cd qwen3-1.7b/deploy
./build-linux.sh

# 板端运行
cd /home/topeet/RKLLM/qwen3-1.7b
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./llm_demo Qwen3-1.7B_W8A8_RK3588.rkllm 4096 4096
```

详见 `02.rknn-llm部署.txt`。

#### 三、YOLO + LLM 摄像头联调（yolo_llm_camera）

```bash
# 编译
cd yolo_llm_camera
./build-linux.sh

# 推送后，板端运行（USB 摄像头一般为 /dev/video21）
cd /home/topeet/RKLLM/yolo_llm/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./yolo_llm_camera_demo model/RK3588/yolo26n.rknn ../Qwen3-1.7B_W8A8_RK3588.rkllm /dev/video21 256 4096
```

- 窗口左上角：**FPS**、**Latency (ms)**、**objs + 类别名**
- 终端：约每秒 `[Vision] N objs [...] FPS Latency`
- **空格**：问「摄像头看到了什么？」；每次回答后打印 **Performance Metrics**
- **q**：退出

详见 [yolo_llm_camera/README.md](yolo_llm_camera/README.md) 与 `02.rknn-llm部署.txt` 第四节。

#### 四、多模态 Token 注入（multimodal-token-injection）

```bash
# 生成映射表（可选，需 transformers）
cd multimodal-token-injection
python3 scripts/generate_yolo_qwen_token_map.py --qwen-dir ../qwen3-1.7b/Qwen3-1.7B --output include/yolo_qwen_token_map.h

# 编译
./build-linux.sh

# 板端运行（可选末尾加类别 ID，如 0 2 15 16）
cd /home/topeet/RKLLM/qwen3-1.7b/demo_Linux_aarch64
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./multimodal_token_injection_demo ../Qwen3-1.7B_W8A8_RK3588.rkllm 256 4096
```

详见 [multimodal-token-injection/README.md](multimodal-token-injection/README.md)。

### multimodal-token-injection 与 yolo_llm_camera 是否可合并？

**可以合并**，两种常见做法：

| 项目 | 作用 | 输入 LLM 的方式 |
|------|------|------------------|
| **multimodal-token-injection** | 仅 LLM + Token 注入（无摄像头、无 YOLO） | 命令行 class_id → token id → `RKLLM_INPUT_TOKEN` |
| **yolo_llm_camera** | 摄像头 + YOLO + LLM 联调 | 检测类别名拼成文本 → `RKLLM_INPUT_PROMPT` |

- **轻量合并（推荐）**：在 yolo_llm_camera 中增加 `--token-injection` 模式，按空格时用 class_id→token id 直通 LLM；multimodal-token-injection 保留为独立小 demo。
- **完全合并**：只保留 yolo_llm_camera，支持文本 prompt、token 注入、无摄像头仅 token 注入三种模式。

### 依赖与环境

- **板子**：RK3588/RK3588S，带 RKNN/RKLLM 驱动与运行时
- **交叉编译**：aarch64 gcc、CMake；YOLO 需 OpenCV、RGA、RKNN；LLM 需 RKLLM（`librkllm_api`）
- **模型**：YOLO 的 `.rknn` 与 `model/coco_80_labels_list.txt`；Qwen3-1.7B 的 `.rkllm`
- 详细环境、adb 推送与排错见 **`02.rknn-llm部署.txt`**

### 文档索引

| 文档 | 说明 |
|------|------|
| [02.rknn-llm部署.txt](02.rknn-llm部署.txt) | 整体部署与 adb |
| [yolo-rk3588/rknn-cpp-Multithreading/README.md](yolo-rk3588/rknn-cpp-Multithreading/README.md) | 纯 YOLO |
| [yolo_llm_camera/README.md](yolo_llm_camera/README.md) | YOLO+LLM 摄像头 |
| [multimodal-token-injection/README.md](multimodal-token-injection/README.md) | Token 直通多模态 |
