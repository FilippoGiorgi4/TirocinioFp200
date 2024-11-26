cmake_minimum_required(VERSION 3.16)
project(tflite_times C CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(HOME $ENV{HOME})

set(TENSORFLOW_SOURCE_DIR "${HOME}/tensorflow_src")

if(NOT TENSORFLOW_SOURCE_DIR)
  get_filename_component(TENSORFLOW_SOURCE_DIR
    "${CMAKE_CURRENT_LIST_DIR}/../../../../" ABSOLUTE)
endif()

add_subdirectory(
  "${TENSORFLOW_SOURCE_DIR}/tensorflow/lite"
  "${CMAKE_CURRENT_BINARY_DIR}/tensorflow-lite" EXCLUDE_FROM_ALL)

add_executable(tflite_inference tflite_inference.c)
target_link_libraries(tflite_inference tensorflow-lite)