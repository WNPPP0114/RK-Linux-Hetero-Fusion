# 端侧多模态推理（Token Injection）

在 RK3588 上实现 **语义 Token 直通**：YOLO 检测类别 ID 与 Qwen3-1.7B 词表 ID 直接映射，跳过 Tokenizer 与 JSON，用 `RKLLM_INPUT_TOKEN` 输入，目标推理速度约 **14 Tokens/s**。

## 思路

- **YOLO**（已部署）：输出每帧的 `class_id`（0..79，COCO 80 类）。
- **映射表**：`yolo_qwen_token_map.h` 中 `YOLO_CLASS_TO_QWEN_TOKEN_ID[80]` 为每个 class_id 对应一个 Qwen token id。
- **LLM**：Qwen3-1.7B（W8A8）使用 `RKLLM_INPUT_TOKEN`，输入 = prefix token 序列 + 检测到的类别 token 序列 + suffix token 序列，无需运行时 Tokenizer 或 JSON 拼接。

## 目录

- `scripts/generate_yolo_qwen_token_map.py`：根据 Qwen 词表与 COCO 80 类名生成映射头文件；若安装 `transformers`，会同时生成 prefix/suffix token 序列。
- `include/yolo_qwen_token_map.h`：生成结果（可提交或构建前运行脚本生成）。
- `src/multimodal_token_injection_demo.cpp`：仅 LLM + Token 注入的 demo（不跑 YOLO，用命令行传入 class_id 或默认示例）。

## 依赖

- RKLLM Runtime（与 qwen3-1.7b 部署相同，如 `librkllmrt.so`）。
- 若需在 PC 上生成完整 prefix/suffix：`pip install transformers`，并指定 `--qwen-dir` 指向 Qwen3-1.7B 目录。

## 生成映射表

```bash
# 使用仓库内 Qwen3-1.7B
python3 scripts/generate_yolo_qwen_token_map.py

# 指定 Qwen 目录与输出路径
python3 scripts/generate_yolo_qwen_token_map.py --qwen-dir /path/to/Qwen3-1.7B --output include/yolo_qwen_token_map.h
```

生成后 `include/yolo_qwen_token_map.h` 会包含：

- `YOLO_CLASS_TO_QWEN_TOKEN_ID[80]`：class_id → token_id。
- `PREFIX_TOKEN_IDS` / `SUFFIX_TOKEN_IDS`（在安装 transformers 时）：与 Qwen chat 格式一致的前后缀 token 序列。

## 进入目录与执行指令

### 1. 生成映射表（PC，可选）

```bash
cd multimodal-token-injection
python3 scripts/generate_yolo_qwen_token_map.py --qwen-dir ../qwen3-1.7b/Qwen3-1.7B --output include/yolo_qwen_token_map.h
```

### 2. 编译（PC，aarch64 交叉编译）

```bash
cd multimodal-token-injection
./build-linux.sh
```

产物在 `install/demo_Linux_aarch64/`。需在 `CMakeLists.txt` 中设置 `RKLLM_API_PATH` 指向本机 `librkllm_api`。

### 3. 推送到开发板（PC 执行 adb）

```bash
adb shell mkdir -p /home/topeet/RKLLM/qwen3-1.7b
adb push multimodal-token-injection/install/demo_Linux_aarch64/ /home/topeet/RKLLM/qwen3-1.7b/
adb push qwen3-1.7b/export/Qwen3-1.7B_W8A8_RK3588.rkllm /home/topeet/RKLLM/qwen3-1.7b/
```

### 4. 开发板运行

**进入目录：**

```bash
cd /home/topeet/RKLLM/qwen3-1.7b/demo_Linux_aarch64
```

**执行指令：**

```bash
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./multimodal_token_injection_demo ../Qwen3-1.7B_W8A8_RK3588.rkllm 256 4096
```

指定类别 ID 示例（0=person, 1=bicycle, 2=car, …）：

```bash
./multimodal_token_injection_demo ../Qwen3-1.7B_W8A8_RK3588.rkllm 256 4096 0 2 15 16
```

## 编译 Demo（手动）

```bash
cd multimodal-token-injection
mkdir -p build && cd build
cmake .. -DRKLLM_API_PATH=/path/to/librkllm_api -DCMAKE_C_COMPILER=... -DCMAKE_CXX_COMPILER=...
make && make install
```

## 运行 Demo（参数说明）

- 第 1 参数：LLM 模型路径（如 `../Qwen3-1.7B_W8A8_RK3588.rkllm`）。
- 第 2/3 参数：`max_new_tokens`、`max_context_len`（如 256、4096）。
- 后续可选：COCO 类别 ID（0..79），不传则用默认 person,bicycle,car,cat,dog。

## 与 YOLO 联调（Pipeline）

当前 demo 仅做 **LLM + Token 注入**。若要和已部署的 YOLO 串联：

1. **YOLO 侧**：已为 `detect_result_t` 增加 `class_id` 字段（见 `yolo-rk3588/rknn-cpp-Multithreading` 的 postprocess），后处理会填充每个检测框的 `class_id`。
2. **本仓库**：从 YOLO 得到 `detect_result_group_t` 后，收集 `results[i].class_id`（可去重），用 `YOLO_CLASS_TO_QWEN_TOKEN_ID[class_id]` 得到 token id，再拼成 prefix + token_ids + suffix 调用 `rkllm_run(..., RKLLM_INPUT_TOKEN, ...)`。

可将 YOLO 与本 demo 放在同一进程（链接 YOLO 的 rknn + postprocess）或通过 IPC/文件传递 class_id 列表。

## RKLLM Token 输入结构

若你使用的 `rkllm.h` 中 `RKLLMInput` 的 token 输入字段名不同（例如 `input_d.token_ids` / `n_token` 等），请按实际头文件修改 `multimodal_token_injection_demo.cpp` 中的：

```cpp
rkllm_input.token_input.token_ids = ...;
rkllm_input.token_input.n_token = ...;
```

## 性能与输出

- 端到端：约 **14 Tokens/s**（与 Qwen 1.8B 在 RK3588 上的公开数据量级一致）。
- 运行结束时会在终端打印：Total 时间、TTFT（首 token 延迟）、Token 数、E2E/Decode 速度（tokens/s）。
