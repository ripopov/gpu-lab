cmake_minimum_required(VERSION 3.25)
set(CMAKE_HIP_ARCHITECTURES gfx942 CACHE STRING "Target GPU architectures")
project(gpu-lab-samples LANGUAGES HIP)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SDK_IMAGE "gem5-gpu-hsakmt-sdk:local" CACHE STRING "Installed SDK image")
set(RUNNER_IMAGE "gem5-gpu-hsakmt-runner:local" CACHE STRING "Installed runner image")
# Host builds pass the resolved image ID. Recompile if that SDK changes.
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/sdk-image.txt" CONTENT "@SDK_IMAGE@\n" @ONLY)
enable_testing()

foreach(sample hip-vector-add hip-memcopy)
    set_source_files_properties("${sample}.cpp" PROPERTIES
        LANGUAGE HIP
        OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/sdk-image.txt")
    add_executable("${sample}" "${sample}.cpp" hip-check.hpp)
    set_target_properties("${sample}" PROPERTIES
        HIP_STANDARD 17
        HIP_STANDARD_REQUIRED YES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_compile_options("${sample}" PRIVATE -Wall -Wextra)

    # Host CTest launches these: run.sh coordinates the Docker containers.
    foreach(backend simdojo gem5-kmd)
        add_test(NAME "${backend}.${sample}"
            COMMAND bash "${CMAKE_SOURCE_DIR}/run.sh" --no-build
                "${backend}" "${sample}" "${CMAKE_BINARY_DIR}")
        set_tests_properties("${backend}.${sample}" PROPERTIES
            LABELS "${backend}" TIMEOUT 480
            ENVIRONMENT "SDK_IMAGE=${SDK_IMAGE};RUNNER_IMAGE=${RUNNER_IMAGE}")
    endforeach()
endforeach()
