#!/bin/bash
# 交叉编译 aarch64（RK3588），需与 yolo / qwen 使用相同工具链
if [[ -z ${BUILD_TYPE} ]]; then BUILD_TYPE=Release; fi
GCC_COMPILER_PATH=/usr/local/arm64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu
C_COMPILER=${GCC_COMPILER_PATH}-gcc
CXX_COMPILER=${GCC_COMPILER_PATH}-g++
TARGET_ARCH=aarch64
TARGET_PLATFORM=linux_${TARGET_ARCH}

ROOT_PWD=$(cd "$(dirname "$0")" && pwd)
BUILD_DIR=${ROOT_PWD}/build/build_${TARGET_PLATFORM}_${BUILD_TYPE}
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake ../.. \
  -DCMAKE_SYSTEM_PROCESSOR=${TARGET_ARCH} \
  -DCMAKE_SYSTEM_NAME=Linux \
  -DCMAKE_C_COMPILER=${C_COMPILER} \
  -DCMAKE_CXX_COMPILER=${CXX_COMPILER} \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

make -j4
make install
