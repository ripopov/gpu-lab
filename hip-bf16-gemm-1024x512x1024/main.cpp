#include "bf16-gemm.hpp"

#include <cmath>

namespace {

using namespace bf16_gemm;
constexpr int K = 1024;

__global__ void gemm(const Bf16* a, const Bf16* b, const float* c, float* d)
{
    const int row = blockIdx.y * blockDim.y + threadIdx.y;
    const int col = blockIdx.x * blockDim.x + threadIdx.x;
    const int out = row * N + col;

    float acc = c[out];
    for (int k = 0; k < K; ++k) {
        acc = fmaf(static_cast<float>(a[row * K + k]),
                   static_cast<float>(b[k * N + col]), acc);
    }
    d[out] = acc;
}

void launch(const Bf16* a, const Bf16* b, const float* c, float* d)
{
    constexpr int block_size = 16;
    static_assert(M % block_size == 0 && N % block_size == 0);
    const dim3 block(block_size, block_size);
    const dim3 grid(N / block_size, M / block_size);
    gemm<<<grid, block>>>(a, b, c, d);
}

} // namespace

int main()
{
    return bf16_gemm::run<K>("hip-bf16-gemm-1024x512x1024", launch);
}
