#include "hip-check.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

__global__ void memory_copy(const std::uint32_t* source, std::uint32_t* destination, unsigned size)
{
    unsigned i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i < size) {
        destination[i] = source[i];
    }
}

int main()
{
    constexpr unsigned size = 4099;
    constexpr unsigned threads = 64;
    constexpr unsigned bytes = size * sizeof(std::uint32_t);
    std::vector<std::uint32_t> source(size), result(size);
    for (unsigned i = 0; i < size; ++i) {
        source[i] = (i + 1) ^ 0x5a5a1234u;
    }

    std::uint32_t *device_source, *device_destination;
    check(hipMalloc(&device_source, bytes));
    check(hipMalloc(&device_destination, bytes));
    check(hipMemcpy(device_source, source.data(), bytes, hipMemcpyHostToDevice));

    memory_copy<<<(size + threads - 1) / threads, threads>>>(device_source, device_destination, size);
    check(hipGetLastError());
    check(hipDeviceSynchronize());
    check(hipMemcpy(result.data(), device_destination, bytes, hipMemcpyDeviceToHost));
    check(hipFree(device_source));
    check(hipFree(device_destination));

    if (result != source) {
        std::cerr << "hip-memcopy: FAIL\n";
        return 1;
    }
    std::cout << "hip-memcopy: PASS\n";
}
