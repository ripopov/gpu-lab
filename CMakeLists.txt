cmake_minimum_required(VERSION 3.25)
set(CMAKE_HIP_ARCHITECTURES gfx942 CACHE STRING "Target GPU architectures")
project(gpu-lab-samples LANGUAGES HIP)

include(cmake/HipSamples.cmake)

add_hip_sample(hip-vector-add)
add_hip_sample(hip-memcopy)
add_hip_sample(hip-julia-fractal)
add_hip_sample(hip-bf16-gemm-1024x512x1024)
set_tests_properties(gem5-kmd.hip-bf16-gemm-1024x512x1024 PROPERTIES TIMEOUT 0)
