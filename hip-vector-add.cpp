#include "hip-check.hpp"

#include <iostream>
#include <vector>

__global__ void vector_add(const float* a, const float* b, float* c, unsigned size)
{
    unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < size) {
        c[i] = a[i] + b[i];
    }
}

int main()
{
    constexpr unsigned size = 257;
    constexpr unsigned threads = 64;
    constexpr unsigned bytes = size * sizeof(float);
    std::vector<float> a(size), b(size), result(size);
    for (unsigned i = 0; i < size; ++i) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(2 * i);
    }

    float *device_a, *device_b, *device_c;
    check(hipMalloc(&device_a, bytes));
    check(hipMalloc(&device_b, bytes));
    check(hipMalloc(&device_c, bytes));
    check(hipMemcpy(device_a, a.data(), bytes, hipMemcpyHostToDevice));
    check(hipMemcpy(device_b, b.data(), bytes, hipMemcpyHostToDevice));

    vector_add<<<(size + threads - 1) / threads, threads>>>(device_a, device_b, device_c, size);
    check(hipGetLastError());
    check(hipDeviceSynchronize());
    check(hipMemcpy(result.data(), device_c, bytes, hipMemcpyDeviceToHost));
    check(hipFree(device_a));
    check(hipFree(device_b));
    check(hipFree(device_c));

    for (unsigned i = 0; i < size; ++i) {
        if (result[i] != a[i] + b[i]) {
            std::cerr << "hip-vector-add: FAIL at " << i << '\n';
            return 1;
        }
    }
    std::cout << "hip-vector-add: PASS\n";
}
