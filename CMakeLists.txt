cmake_minimum_required(VERSION 3.25)
project(gpu-lab-samples LANGUAGES NONE)

enable_testing()
find_program(DOCKER docker REQUIRED)
find_program(BASH bash REQUIRED)

set(SDK_IMAGE "gem5-gpu-hsakmt-sdk:local" CACHE STRING "Installed SDK image")
set(RUNNER_IMAGE "gem5-gpu-hsakmt-runner:local" CACHE STRING "Installed runner image")

foreach(kind SDK RUNNER)
    execute_process(
        COMMAND "${DOCKER}" image inspect "${${kind}_IMAGE}" --format "{{.Id}}"
        OUTPUT_VARIABLE ${kind}_ID
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE error
        RESULT_VARIABLE status
        TIMEOUT 15
    )
    if(NOT status STREQUAL "0")
        message(FATAL_ERROR "Install ${${kind}_IMAGE} locally before configuring.\n${error}")
    endif()
    execute_process(
        COMMAND "${DOCKER}" image inspect "${${kind}_ID}"
        OUTPUT_VARIABLE metadata
        COMMAND_ERROR_IS_FATAL ANY
        TIMEOUT 15
    )
    string(JSON os GET "${metadata}" 0 Os)
    string(JSON arch GET "${metadata}" 0 Architecture)
    string(JSON ${kind}_REVISION GET "${metadata}" 0 Config Labels org.gem5.rocjitsu.revision)
    set(${kind}_PLATFORM "${os}/${arch}")
    message(STATUS "${kind}: ${${kind}_IMAGE} (${${kind}_PLATFORM})")
endforeach()

if(NOT SDK_PLATFORM STREQUAL RUNNER_PLATFORM)
    message(FATAL_ERROR "SDK and runner image platforms must match.")
endif()
if(NOT SDK_REVISION STREQUAL RUNNER_REVISION)
    message(FATAL_ERROR "SDK and runner rocjitsu revisions must match.")
endif()

execute_process(COMMAND id -u OUTPUT_VARIABLE uid OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND id -g OUTPUT_VARIABLE gid OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

foreach(sample hip-vector-add hip-memcopy)
    set(binary "${CMAKE_BINARY_DIR}/bin/${sample}")
    add_custom_command(
        OUTPUT "${binary}"
        COMMAND "${DOCKER}" run --rm --pull never --network none
            --platform "${SDK_PLATFORM}" --user "${uid}:${gid}" -e HOME=/tmp
            -v "${CMAKE_SOURCE_DIR}:/source:ro"
            -v "${CMAKE_BINARY_DIR}:/build" -w /build
            "${SDK_ID}" hipcc --offload-arch=gfx942 -O2 -std=c++17 -Wall -Wextra
            "/source/${sample}.cpp" -o "/build/bin/${sample}"
        DEPENDS "${sample}.cpp" hip-check.hpp "${CMAKE_BINARY_DIR}/CMakeCache.txt"
        VERBATIM
    )
    add_custom_target("${sample}" ALL DEPENDS "${binary}")

    foreach(backend simdojo gem5-kmd)
        add_test(
            NAME "${backend}.${sample}"
            COMMAND "${BASH}" "${CMAKE_SOURCE_DIR}/run.sh"
                "${backend}" "${SDK_ID}" "${RUNNER_ID}" "${SDK_PLATFORM}"
                "${binary}" "${CMAKE_BINARY_DIR}/results/${backend}/${sample}"
        )
        set_tests_properties("${backend}.${sample}" PROPERTIES LABELS "${backend}" TIMEOUT 480)
    endforeach()
endforeach()
