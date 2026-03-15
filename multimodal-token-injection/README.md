# On-Device Multimodal Inference (Token Injection)

中文 / Chinese: [README_CN.md](README_CN.md)

Implements **Semantic Token Passthrough** on RK3588: YOLO detection class IDs map directly to Qwen3-1.7B vocabulary IDs, skipping the Tokenizer and JSON formatting, inputting via `RKLLM_INPUT_TOKEN`, targeting an inference speed of ~**14 Tokens/s**.

## Approach

- **YOLO** (already deployed): Outputs `class_id` per frame (0..79, COCO 80 classes).
- **Mapping Table**: `YOLO_CLASS_TO_QWEN_TOKEN_ID[80]` in `yolo_qwen_token_map.h` corresponds each class_id to a Qwen token id.
- **LLM**: Qwen3-1.7B (W8A8) uses `RKLLM_INPUT_TOKEN`, where input = prefix token sequence + detected class token sequence + suffix token sequence, eliminating runtime Tokenizer or JSON overhead.

## Directory Structure

- `scripts/generate_yolo_qwen_token_map.py`: Generates the mapping header based on Qwen vocabulary and COCO 80 class names; if `transformers` is installed, it also generates prefix/suffix token sequences.
- `include/yolo_qwen_token_map.h`: Generation result (can be committed or generated via script before building).
- `src/multimodal_token_injection_demo.cpp`: Demo for LLM + Token injection only (does not run YOLO, takes class_ids from command line or uses default examples).

## Dependencies

- RKLLM Runtime (same as environment setup in `SDK_Chg`, e.g., `librkllmrt.so`).
- To generate full prefix/suffix on PC: `pip install transformers`, and specify `--qwen-dir` pointing to the Qwen3-1.7B directory.

## Generate Mapping Table

```bash
# Using Qwen3-1.7B within the repo
python3 scripts/generate_yolo_qwen_token_map.py

# Specifying Qwen directory and output path
python3 scripts/generate_yolo_qwen_token_map.py --qwen-dir /path/to/Qwen3-1.7B --output include/yolo_qwen_token_map.h
```

After generation, `include/yolo_qwen_token_map.h` will contain:

- `YOLO_CLASS_TO_QWEN_TOKEN_ID[80]`: class_id → token_id.
- `PREFIX_TOKEN_IDS` / `SUFFIX_TOKEN_IDS` (when transformers is installed): Prefix/suffix token sequences consistent with Qwen chat format.

## Directory & Commands

### 1. Generate Mapping Table (PC, optional)

```bash
cd multimodal-token-injection
python3 scripts/generate_yolo_qwen_token_map.py --qwen-dir /path/to/Qwen3-1.7B --output include/yolo_qwen_token_map.h
```

### 2. Build (PC, aarch64 cross-compile)

```bash
cd multimodal-token-injection
./build-linux.sh
```

Artifacts are in `install/demo_Linux_aarch64/`. Ensure `RKLLM_API_PATH` in `CMakeLists.txt` points to your local `librkllm_api`.

### 3. Push to Board (PC running adb)

```bash
adb shell mkdir -p /home/admin/RKLLM/token_injection
adb push multimodal-token-injection/install/demo_Linux_aarch64/ /home/admin/RKLLM/token_injection/
adb push /path/to/your/Qwen3-1.7B_W8A8_RK3588.rkllm /home/admin/RKLLM/token_injection/
```

### 4. Run on Board

**Enter directory:**

```bash
cd /home/admin/RKLLM/token_injection/demo_Linux_aarch64
```

**Execute command:**

```bash
ulimit -HSn 102400
export LD_LIBRARY_PATH=./lib
./multimodal_token_injection_demo ../Qwen3-1.7B_W8A8_RK3588.rkllm 256 4096
```

Example specifying class IDs (0=person, 1=bicycle, 2=car, …):

```bash
./multimodal_token_injection_demo ../Qwen3-1.7B_W8A8_RK3588.rkllm 256 4096 0 2 15 16
```

## Build Demo (Manual)

```bash
cd multimodal-token-injection
mkdir -p build && cd build
cmake .. -DRKLLM_API_PATH=/path/to/librkllm_api -DCMAKE_C_COMPILER=... -DCMAKE_CXX_COMPILER=...
make && make install
```

## Run Demo (Parameter Description)

- 1st Parameter: LLM model path (e.g., `../Qwen3-1.7B_W8A8_RK3588.rkllm`).
- 2nd/3rd Parameters: `max_new_tokens`, `max_context_len` (e.g., 256, 4096).
- Subsequent optional: COCO class IDs (0..79), defaults to person, bicycle, car, cat, dog if omitted.

## Integration with YOLO (Pipeline)

The current demo only handles **LLM + Token injection**. To chain with deployed YOLO:

1. **YOLO Side**: The `detect_result_t` already has a `class_id` field (see postprocess in `yolo-rk3588/rknn-cpp-Multithreading`); the post-processing fills the `class_id` for each bounding box.
2. **This Repo**: Upon receiving `detect_result_group_t` from YOLO, collect `results[i].class_id` (can deduplicate), get token ids using `YOLO_CLASS_TO_QWEN_TOKEN_ID[class_id]`, concatenate prefix + token_ids + suffix, and call `rkllm_run(..., RKLLM_INPUT_TOKEN, ...)`.

You can place YOLO and this demo in the same process (linking YOLO's rknn + postprocess) or pass the class_id list via IPC/file.

## RKLLM Token Input Structure

If the token input field names in `RKLLMInput` within your `rkllm.h` differ (e.g., `input_d.token_ids` / `n_token` etc.), modify `multimodal_token_injection_demo.cpp` accordingly:

```cpp
rkllm_input.token_input.token_ids = ...;
rkllm_input.token_input.n_token = ...;
```

## Performance & Output

- End-to-end: ~**14 Tokens/s** (in line with published data for Qwen 1.8B on RK3588).
- Prints to terminal upon completion: Total time, TTFT (Time To First Token), Token count, E2E/Decode speed (tokens/s).