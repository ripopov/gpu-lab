cmake_minimum_required(VERSION 3.25)
set(CMAKE_HIP_ARCHITECTURES gfx942 CACHE STRING "Target GPU architectures")
project(gpu-lab-samples LANGUAGES HIP)

include(cmake/HipSamples.cmake)

add_hip_sample(hip-vector-add)
add_hip_sample(hip-memcopy)
