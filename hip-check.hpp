#pragma once

#include <hip/hip_runtime.h>

#include <cstdio>
#include <cstdlib>

inline void check(hipError_t status)
{
    if (status != hipSuccess) {
        std::fprintf(stderr, "HIP error: %s\n", hipGetErrorString(status));
        std::exit(EXIT_FAILURE);
    }
}
