#pragma once

#include "hip-check.hpp"

#include <hip/hip_bfloat16.h>

#include <cstdlib>
#include <iostream>
#include <vector>

namespace bf16_gemm {

constexpr int M = 1024;
constexpr int N = 512;
using Bf16 = hip_bfloat16;

template <int K>
inline bool verify(const char* sample, const std::vector<Bf16>& a, const std::vector<Bf16>& b,
                   const std::vector<float>& c, const std::vector<float>& d)
{
    const std::vector<float> b_fp32(b.begin(), b.end());
    std::vector<double> expected(c.begin(), c.end());
    for (int row = 0; row < M; ++row) {
        auto* output_row = expected.data() + row * N;
        for (int k = 0; k < K; ++k) {
            const double lhs = static_cast<float>(a[row * K + k]);
            const auto* input_row = b_fp32.data() + k * N;
            for (int col = 0; col < N; ++col) {
                output_row[col] += lhs * input_row[col];
            }
        }
    }

    // All partial sums are small multiples of 1/256 and are exact in FP32.
    for (int i = 0; i < M * N; ++i) {
        if (d[i] != expected[i]) {
            std::cerr << sample << ": FAIL at (" << i / N << ", " << i % N
                      << "), expected " << expected[i] << ", got " << d[i] << '\n';
            return false;
        }
    }
    return true;
}

template <int K, class Launch>
int run(const char* sample, Launch launch)
{
    std::vector<Bf16> a(M * K), b(K * N);
    std::vector<float> c(M * N), d(M * N);
    for (int row = 0; row < M; ++row) {
        for (int k = 0; k < K; ++k) {
            a[row * K + k] = Bf16(((3 * row + 5 * k + row * k) % 19 - 9) / 16.0f);
        }
        for (int col = 0; col < N; ++col) {
            c[row * N + col] = (1 + (7 * row + 11 * col) % 127) / 256.0f;
        }
    }
    for (int k = 0; k < K; ++k) {
        for (int col = 0; col < N; ++col) {
            b[k * N + col] = Bf16(((7 * k + 3 * col + k * col) % 23 - 11) / 16.0f);
        }
    }

    constexpr auto a_bytes = M * K * sizeof(Bf16);
    constexpr auto b_bytes = K * N * sizeof(Bf16);
    constexpr auto output_bytes = M * N * sizeof(float);
    Bf16 *device_a, *device_b;
    float *device_c, *device_d;
    check(hipMalloc(&device_a, a_bytes));
    check(hipMalloc(&device_b, b_bytes));
    check(hipMalloc(&device_c, output_bytes));
    check(hipMalloc(&device_d, output_bytes));
    check(hipMemcpy(device_a, a.data(), a_bytes, hipMemcpyHostToDevice));
    check(hipMemcpy(device_b, b.data(), b_bytes, hipMemcpyHostToDevice));
    check(hipMemcpy(device_c, c.data(), output_bytes, hipMemcpyHostToDevice));

    std::cout << sample << ": launching GEMM\n" << std::flush;
    launch(device_a, device_b, device_c, device_d);
    check(hipGetLastError());
    check(hipDeviceSynchronize());
    std::cout << sample << ": kernel completed\n" << std::flush;
    check(hipMemcpy(d.data(), device_d, output_bytes, hipMemcpyDeviceToHost));
    check(hipFree(device_a));
    check(hipFree(device_b));
    check(hipFree(device_c));
    check(hipFree(device_d));

    if (!verify<K>(sample, a, b, c, d)) {
        return EXIT_FAILURE;
    }
    std::cout << sample << ": PASS\n";
    return EXIT_SUCCESS;
}

} // namespace bf16_gemm
