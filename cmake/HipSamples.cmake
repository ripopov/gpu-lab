include_guard(GLOBAL)

set(CMAKE_HIP_STANDARD 17)
set(CMAKE_HIP_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(SDK_IMAGE "gem5-gpu-hsakmt-sdk:local" CACHE STRING "Installed SDK image")
set(RUNNER_IMAGE "gem5-gpu-hsakmt-runner:local" CACHE STRING "Installed runner image")
# Host builds pass the resolved image ID. Recompile if that SDK changes.
file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/sdk-image.txt" CONTENT "@SDK_IMAGE@\n" @ONLY)
enable_testing()

function(add_hip_sample sample)
    if(NOT sample MATCHES "^[A-Za-z0-9][A-Za-z0-9_-]*$")
        message(FATAL_ERROR "Sample names must be root directory names containing letters, digits, '_' or '-': ${sample}")
    endif()
    file(GLOB sources CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${sample}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/${sample}/*.hip")
    if(NOT sources)
        message(FATAL_ERROR "Sample '${sample}' needs at least one .cpp or .hip source file")
    endif()
    file(GLOB headers CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/${sample}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/${sample}/*.hpp")
    set_source_files_properties(${sources} PROPERTIES
        LANGUAGE HIP
        OBJECT_DEPENDS "${CMAKE_BINARY_DIR}/sdk-image.txt")
    add_executable("${sample}" ${sources} ${headers} "${PROJECT_SOURCE_DIR}/common/hip-check.hpp")
    target_include_directories("${sample}" PRIVATE "${PROJECT_SOURCE_DIR}/common")
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
endfunction()
