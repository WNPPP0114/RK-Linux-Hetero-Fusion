cmake_minimum_required(VERSION 3.10)
project(yolo_llm_camera CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

# YOLO 工程路径（与 yolo-rk3588 共用 3rdparty、runtime）
set(YOLO_ROOT ${CMAKE_SOURCE_DIR}/../yolo-rk3588/rknn-cpp-Multithreading)
set(RKNN_API_PATH ${YOLO_ROOT}/../../runtime/RK3588/${CMAKE_SYSTEM_NAME}/librknn_api)
set(LIB_ARCH aarch64)
set(RKNN_RT_LIB ${YOLO_ROOT}/include/librknnrt.so)
set(OpenCV_PATH ${YOLO_ROOT}/include/3rdparty/opencv)
set(RGA_PATH ${YOLO_ROOT}/include/3rdparty/rga/RK3588)
set(RGA_LIB ${RGA_PATH}/lib/Linux/${LIB_ARCH}/librga.so)
set(OpenCV_LIBS
  ${OpenCV_PATH}/lib/libopencv_core.so
  ${OpenCV_PATH}/lib/libopencv_highgui.so
  ${OpenCV_PATH}/lib/libopencv_imgproc.so
  ${OpenCV_PATH}/lib/libopencv_videoio.so
  ${OpenCV_PATH}/lib/libopencv_imgcodecs.so
)

include_directories(${RKNN_API_PATH}/include)
include_directories(${YOLO_ROOT}/include)
include_directories(${YOLO_ROOT}/include/3rdparty)
include_directories(${YOLO_ROOT}/include/3rdparty/libdrm/include)
include_directories(${YOLO_ROOT}/include/3rdparty/libdrm/include/libdrm)
include_directories(${OpenCV_PATH}/include/opencv4)
include_directories(${RGA_PATH}/include)

# RKLLM
if(NOT DEFINED RKLLM_API_PATH)
  set(RKLLM_API_PATH "/home/wzp/RKNN/rknn-llm-main/rkllm-runtime/Linux/librkllm_api")
endif()
include_directories(${RKLLM_API_PATH}/include)
set(RKLLM_RT_LIB ${RKLLM_API_PATH}/aarch64/librkllmrt.so)

add_executable(yolo_llm_camera_demo
  src/yolo_llm_camera_demo.cpp
  ${YOLO_ROOT}/src/postprocess.cc
  ${YOLO_ROOT}/src/drm_alloc.cpp
)
target_include_directories(yolo_llm_camera_demo PRIVATE ${YOLO_ROOT}/include)
target_link_libraries(yolo_llm_camera_demo
  ${RKNN_RT_LIB}
  ${RGA_LIB}
  ${OpenCV_LIBS}
  ${RKLLM_RT_LIB}
  dl
)

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR}/install/demo_${CMAKE_SYSTEM_NAME}_${LIB_ARCH})
set_target_properties(yolo_llm_camera_demo PROPERTIES
  INSTALL_RPATH "\$ORIGIN/lib"
  BUILD_WITH_INSTALL_RPATH TRUE
  LINK_FLAGS "-Wl,--disable-new-dtags -Wl,-rpath,'\$ORIGIN/lib'"
)
install(TARGETS yolo_llm_camera_demo DESTINATION ./)
install(PROGRAMS ${RKNN_RT_LIB} ${RGA_LIB} ${RKLLM_RT_LIB} DESTINATION lib)
install(DIRECTORY ${OpenCV_PATH}/lib/ DESTINATION lib FILES_MATCHING PATTERN "*.so*")
if(EXISTS ${YOLO_ROOT}/model)
  install(DIRECTORY ${YOLO_ROOT}/model DESTINATION ./)
endif()
