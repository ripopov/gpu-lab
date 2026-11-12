#include "hip-check.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

// Keep CPU and GPU rounding identical. Only explicit fmaf calls are fused.
#pragma clang fp contract(off)

namespace {
constexpr unsigned width = 320;
constexpr unsigned height = 240;
constexpr unsigned samples = 4; // 2x2 supersampling, interleaved in each thread.
constexpr unsigned iterations = 192;
constexpr unsigned batch_iterations = 8;
constexpr unsigned threads = 128;
constexpr unsigned tile_side = 8; // One gfx942 wave64 per spatial tile.
static_assert(tile_side * tile_side == 64);
static_assert(width % tile_side == 0 && height % tile_side == 0);
static_assert(width * height % threads == 0);
static_assert(iterations % batch_iterations == 0 && threads % 64 == 0);
constexpr float real_c = -0.8f;
constexpr float imag_c = 0.156f;
constexpr float escape_radius2 = 256.0f;

struct Orbit {
    float radius2;
    float trap;
    unsigned steps;
};
static_assert(sizeof(Orbit) == 12);

__host__ __device__ float initial_x(unsigned pixel, unsigned sample)
{
    const float subpixel = (sample & 1) ? 0.75f : 0.25f;
    return fmaf(static_cast<float>(pixel % width) + subpixel, 0.01f, -1.6f);
}

__host__ __device__ float initial_y(unsigned pixel, unsigned sample)
{
    const float subpixel = (sample & 2) ? 0.75f : 0.25f;
    return fmaf(static_cast<float>(pixel / width) + subpixel, -0.01f, 1.2f);
}

__global__ void julia_orbits(Orbit* output)
{
    const unsigned thread = blockIdx.x * blockDim.x + threadIdx.x;
    const unsigned tile = thread / 64;
    const unsigned column = (tile % (width / tile_side)) * tile_side + thread % tile_side;
    const unsigned row = (tile / (width / tile_side)) * tile_side + (thread / tile_side) % tile_side;
    const unsigned pixel = row * width + column;

    float x[samples], y[samples], radius2[samples], trap[samples];
    unsigned steps[samples];
#pragma unroll
    for (unsigned s = 0; s < samples; ++s) {
        x[s] = initial_x(pixel, s);
        y[s] = initial_y(pixel, s);
        radius2[s] = fmaf(x[s], x[s], y[s] * y[s]);
        trap[s] = 4.0f;
        steps[s] = 0;
    }

    // Freeze escaped orbits to keep all arithmetic finite. Check for a whole
    // wave's completion between batches, without divergent per-lane exits.
    // Spatial tiles reduce wasted work where nearby pixels escape quickly.
#pragma unroll 1
    for (unsigned batch = 0; batch < iterations / batch_iterations; ++batch) {
        bool live = false;
#pragma unroll
        for (unsigned s = 0; s < samples; ++s) {
            live |= radius2[s] <= escape_radius2;
        }
        if (!__any(live)) {
            break;
        }
#pragma unroll
        for (unsigned i = 0; i < batch_iterations; ++i) {
#pragma unroll
            for (unsigned s = 0; s < samples; ++s) {
                const bool active = radius2[s] <= escape_radius2;
                const float next_x = fmaf(x[s], x[s], fmaf(-y[s], y[s], real_c));
                const float next_y = fmaf(2.0f * x[s], y[s], imag_c);
                x[s] = active ? next_x : x[s];
                y[s] = active ? next_y : y[s];
                radius2[s] = fmaf(x[s], x[s], y[s] * y[s]);
                trap[s] = fminf(trap[s], fabsf(radius2[s] - 0.5f));
                steps[s] += active;
            }
        }
    }

    // Tile-major within each sample: a wave writes 64 contiguous orbits.
    // Each tile owns whole cache lines, even when adjacent tiles use other CUs.
#pragma unroll
    for (unsigned s = 0; s < samples; ++s) {
        output[s * width * height + thread] = {radius2[s], trap[s], steps[s]};
    }
}

unsigned orbit_index(unsigned pixel, unsigned sample)
{
    const unsigned column = pixel % width;
    const unsigned row = pixel / width;
    const unsigned tile = (row / tile_side) * (width / tile_side) + column / tile_side;
    const unsigned lane = (row % tile_side) * tile_side + column % tile_side;
    return sample * width * height + tile * 64 + lane;
}

// Independent scalar control flow: stop immediately on escape, unlike the GPU.
Orbit reference_orbit(unsigned pixel, unsigned sample)
{
    float x = initial_x(pixel, sample);
    float y = initial_y(pixel, sample);
    Orbit result{fmaf(x, x, y * y), 4.0f, 0};
    while (result.steps < iterations && result.radius2 <= escape_radius2) {
        const float next_x = fmaf(x, x, fmaf(-y, y, real_c));
        y = fmaf(2.0f * x, y, imag_c);
        x = next_x;
        result.radius2 = fmaf(x, x, y * y);
        result.trap = fminf(result.trap, fabsf(result.radius2 - 0.5f));
        ++result.steps;
    }
    return result;
}

std::array<float, 3> color(const Orbit& orbit)
{
    // Transcendentals are only used for host-side presentation, not in the
    // measured GPU recurrence. Dark navy -> teal -> ivory -> copper -> navy.
    constexpr std::array<std::array<float, 3>, 6> palette{{
        {0.015f, 0.025f, 0.09f}, {0.015f, 0.25f, 0.38f},
        {0.12f, 0.72f, 0.78f}, {0.98f, 0.94f, 0.72f},
        {0.85f, 0.32f, 0.08f}, {0.015f, 0.025f, 0.09f}
    }};
    const float glow = 1.0f / (1.0f + 12.0f * std::sqrt(orbit.trap));
    if (orbit.radius2 <= escape_radius2) {
        return {0.025f + 0.12f * glow, 0.04f + 0.38f * glow,
                0.09f + 0.46f * glow};
    }
    const float smooth = orbit.steps + 1.0f -
        std::log2(0.5f * std::log2(orbit.radius2));
    float t = smooth * 0.035f;
    t = (t - std::floor(t)) * (palette.size() - 1);
    const unsigned index = static_cast<unsigned>(t);
    const float fraction = t - index;
    std::array<float, 3> rgb{};
    for (unsigned channel = 0; channel < rgb.size(); ++channel) {
        rgb[channel] = palette[index][channel] * (1.0f - fraction) +
                       palette[index + 1][channel] * fraction;
        rgb[channel] *= 0.7f + 0.3f * glow;
    }
    return rgb;
}

bool write_image(const char* path, const std::vector<Orbit>& orbits)
{
    std::ofstream image(path, std::ios::binary);
    image << "P6\n" << width << ' ' << height << "\n255\n";
    for (unsigned pixel = 0; pixel < width * height; ++pixel) {
        std::array<float, 3> rgb{};
        for (unsigned s = 0; s < samples; ++s) {
            const auto value = color(orbits[orbit_index(pixel, s)]);
            for (unsigned channel = 0; channel < rgb.size(); ++channel) {
                rgb[channel] += value[channel] / samples;
            }
        }
        for (float value : rgb) {
            const auto byte = static_cast<unsigned char>(
                std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
            image.put(static_cast<char>(byte));
        }
    }
    image.close();
    return static_cast<bool>(image);
}
} // namespace

int main()
{
    constexpr unsigned count = width * height * samples;
    constexpr std::size_t bytes = count * sizeof(Orbit);
    std::vector<Orbit> result(count);
    Orbit* device_result;
    check(hipMalloc(&device_result, bytes));
    julia_orbits<<<(width * height + threads - 1) / threads, threads>>>(device_result);
    check(hipGetLastError());
    check(hipDeviceSynchronize());
    check(hipMemcpy(result.data(), device_result, bytes, hipMemcpyDeviceToHost));
    check(hipFree(device_result));

    std::uint64_t useful_steps = 0;
    unsigned bounded = 0;
    for (unsigned s = 0; s < samples; ++s) {
        for (unsigned pixel = 0; pixel < width * height; ++pixel) {
            const Orbit expected = reference_orbit(pixel, s);
            const Orbit actual = result[orbit_index(pixel, s)];
            if (!std::isfinite(actual.radius2) || !std::isfinite(actual.trap) ||
                actual.radius2 != expected.radius2 || actual.trap != expected.trap ||
                actual.steps != expected.steps) {
                std::cerr << "hip-julia-fractal: FAIL at pixel " << pixel
                          << ", sample " << s << ": got (" << actual.radius2
                          << ", " << actual.trap << ", " << actual.steps
                          << "), expected (" << expected.radius2 << ", "
                          << expected.trap << ", " << expected.steps << ")\n";
                return 1;
            }
            useful_steps += actual.steps;
            bounded += actual.radius2 <= escape_radius2;
        }
    }
    // Account for batch rounding and frozen lanes, separately from useful
    // iterations. This also makes padding visible in performance reports.
    std::uint64_t executed_steps = 0;
    for (unsigned row = 0; row < height; row += tile_side) {
        for (unsigned column = 0; column < width; column += tile_side) {
            unsigned longest = 0;
            for (unsigned y = 0; y < tile_side; ++y) {
                for (unsigned x = 0; x < tile_side; ++x) {
                    for (unsigned s = 0; s < samples; ++s) {
                        longest = std::max(longest, result[orbit_index(
                            (row + y) * width + column + x, s)].steps);
                    }
                }
            }
            executed_steps += ((longest + batch_iterations - 1) / batch_iterations) *
                batch_iterations * tile_side * tile_side * samples;
        }
    }
    if (bounded == 0 || bounded == count) {
        std::cerr << "hip-julia-fractal: FAIL: degenerate image\n";
        return 1;
    }
    constexpr const char* path = "hip-julia-fractal.ppm";
    if (!write_image(path, result)) {
        std::cerr << "hip-julia-fractal: FAIL: could not write " << path << '\n';
        return 1;
    }
    std::cout << "hip-julia-fractal: PASS (" << count << " CPU-checked orbits, "
              << width << 'x' << height << ", " << samples << " samples/pixel, "
              << iterations << " max iterations)\n"
              << "Orbit steps: " << useful_steps << " useful / " << executed_steps
              << " executed / " << static_cast<std::uint64_t>(count) * iterations
              << " full budget; bounded samples: " << bounded << '\n'
              << "Image: " << path << '\n';
}
