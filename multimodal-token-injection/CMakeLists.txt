cmake_minimum_required(VERSION 3.10)
project(multimodal_token_injection CXX)

set(CMAKE_CXX_STANDARD 11)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TARGET_LIB_ARCH aarch64)
else()
    set(TARGET_LIB_ARCH armhf)
endif()

# RKLLM：请根据本机路径修改，或通过 -DRKLLM_API_PATH= 传入
if(NOT DEFINED RKLLM_API_PATH)
    set(RKLLM_API_PATH "/home/wzp/RKNN/rknn-llm-main/rkllm-runtime/Linux/librkllm_api")
endif()
if(NOT EXISTS "${RKLLM_API_PATH}/include/rkllm.h")
    message(WARNING "RKLLM include not found at ${RKLLM_API_PATH}/include. Set RKLLM_API_PATH and ensure librkllmrt is available.")
endif()

include_directories(${RKLLM_API_PATH}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(multimodal_token_injection_demo src/multimodal_token_injection_demo.cpp)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(RKLLM_RT_LIB "${RKLLM_API_PATH}/aarch64/librkllmrt.so")
    if(EXISTS "${RKLLM_RT_LIB}")
        target_link_libraries(multimodal_token_injection_demo ${RKLLM_RT_LIB})
    else()
        target_link_libraries(multimodal_token_injection_demo rkllmrt)
    endif()
else()
    target_link_libraries(multimodal_token_injection_demo rkllmrt)
endif()

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/demo_${CMAKE_SYSTEM_NAME}_${TARGET_LIB_ARCH})
install(TARGETS multimodal_token_injection_demo DESTINATION ./)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "${RKLLM_RT_LIB}")
    install(PROGRAMS ${RKLLM_RT_LIB} DESTINATION lib)
endif()
