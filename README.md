# GPU lab samples

Self-checking HIP examples targeting gfx942 (MI300X):

- **hip-vector-add** adds two vectors.
- **hip-memcopy** copies an integer buffer with a GPU kernel.
- **[hip-julia-fractal](hip-julia-fractal/README.md)** renders a supersampled
  320×240 Julia fractal.
- **hip-bf16-gemm-1024x512x1024** computes a BF16 GEMM with a scalar FP32 accumulator per output.

All validate their GPU results and return a nonzero status on failure.
The vector-add and memory-copy sizes exercise multiple blocks and a partially
filled final block.

## Project layout

Each sample has its own directory at the project root:

```text
hip-vector-add/
  main.cpp
hip-memcopy/
  main.cpp
hip-julia-fractal/
  main.cpp
  README.md
common/
  hip-check.hpp
```

To add a sample, create a root directory containing its `.cpp` or `.hip` sources
and add one line to the root `CMakeLists.txt`:

```cmake
add_hip_sample(my-sample)
```

The shared setup lives in `cmake/HipSamples.cmake`; the root CMake file only
declares the project and lists samples.

`add_hip_sample()` collects sources and `.h`/`.hpp` headers directly in that directory,
applies the shared HIP compiler settings and includes `common/`, and registers
both simulator tests. Source additions and removals trigger CMake reconfiguration
on the next build. No per-sample CMake file is needed. Use letters, digits, `_`
and `-` in sample names, starting with a letter or digit.

Build targets and run commands use the directory name:

```sh
./build.sh my-sample
./run.sh simdojo my-sample
./run.sh gem5-kmd my-sample
```

`./test.sh` automatically includes the new sample. To add a named CLion Run
configuration, copy an existing Shell Script configuration and change its sample
argument.

## Requirements

Use a Linux host with a local Docker Engine, Docker CLI, CMake 3.25+ (for
host CTest), Bash and GNU coreutils. Your user must have permission to run
Docker. No host ROCm installation or physical GPU is needed.

Install these existing images locally:

| Image | Purpose |
| --- | --- |
| `gem5-gpu-hsakmt-sdk:local` | ROCm compilation and rocjitsu execution |
| `gem5-gpu-hsakmt-runner:local` | gem5 HSAKMT service |

The SDK must contain CMake 3.25+, Ninja, ROCm Clang and the HIP CMake packages.
The supplied image contains all of them. Host scripts check matching image
platforms and rocjitsu revisions, and resolve tags to immutable image IDs.
Images are never built or pulled by this project.

## CLion setup

1. Open this directory as a CMake project.
2. Under **Settings → Build, Execution, Deployment → Toolchains**, add a
   **Docker** toolchain named **Docker** using `gem5-gpu-hsakmt-sdk:local`.
   Use the container's CMake and Ninja. If C/C++ compiler autodetection needs
   help, the supplied SDK has `/usr/lib/llvm-22/bin/clang` and `clang++`.
   CMake detects the HIP compiler independently through `hipconfig`.
3. Reload CMake. The preset selects the toolchain named **Docker** through its
   `jetbrains.com/clion` vendor field; preset-controlled toolchain fields in
   Settings are read-only. If you name the toolchain differently, update that
   field in `CMakePresets.json`. Configuration must run inside the SDK: a host
   profile without ROCm cannot configure this project.
4. Choose a shared run configuration such as **hip-memcopy / simdojo** or
   **hip-memcopy / gem5-kmd**, and click Run. These are host Shell Script
   configurations; enable CLion's Shell Script plugin if needed.

All samples are real `add_executable` targets with their sources compiled
as HIP. CLion's Build action builds those targets in the SDK. HIP-specific
editor diagnostics still depend on CLion's language-engine support.

The shared Run configurations invoke `run.sh` on the host. It incrementally
builds the selected sample inside the SDK before launching the backend, so
there is no separate build-before-run task to configure. Use these configurations
for simulator execution; a plain CMake Application would launch the binary
directly rather than through rocjitsu.

CLion uses `build/ide`, while the host scripts use `build`. Separate caches
avoid conflicts between CLion's container mount paths and the host paths used
by the scripts. Both builds use the same CMake targets and preset options.

## Build and run from the host

```sh
./build.sh
./build.sh hip-memcopy
./run.sh simdojo hip-memcopy
./run.sh gem5-kmd hip-vector-add
./test.sh                 # build and run all tests
./test.sh simdojo         # build and run one backend
./test.sh gem5-kmd
```

`build.sh` accepts one or more sample names. With no arguments it builds all
samples; leading options are forwarded to `cmake --build` (for example,
`./build.sh --clean-first`).

The root CMake project uses native HIP, not Docker custom compilation commands.
Run CMake directly only in an SDK environment, or use the host scripts above.

**simdojo** uses rocjitsu's functional simulator in the SDK.
**gem5-kmd** connects the SDK application to the runner's gem5 timing model.
The host coordinates both containers; the build container needs no Docker socket.
Each test has its own containers and runtime files, and checks that the runner
exits successfully. Containers are removed on completion or failure.

The `hip-bf16-gemm-1024x512x1024` run on the eight-CU gem5-kmd model took 50 min 27 s of host wall time, including output verification.
It runs without an application timeout.

Run containers use Docker's `--init` to reap detached rocjitsu memory keepers.
For gem5-kmd, the SDK container also joins the runner's private PID namespace
with `--pid=container:<runner>`. This makes socket peer PIDs visible in both
directions, allowing rocjitsu to authorize daemon access to application memory.
The host PID namespace, capabilities, and default security profiles are unchanged.

Some rocjitsu versions still print the memory-keeper child-wait warning when
an application is a direct child of PID 1, even with a reaping init present.
That warning is conservative; `--init` supplies the required reaper. The
separate "no usable peer credentials" warning should not occur with the shared
PID namespace.

Executables are in `build/bin/`. Logs and gem5 statistics are in
`build/results/<backend>/<example>/`. To rerun without rebuilding:

```sh
./run.sh --no-build simdojo hip-memcopy
ctest --test-dir build --output-on-failure
```

To record a VTR trace, add `--vtr` before the backend (gem5-kmd only):

```sh
./run.sh --vtr gem5-kmd hip-julia-fractal
./run.sh --no-build --vtr gem5-kmd hip-vector-add
```

The trace is written to `build/results/gem5-kmd/<sample>/trace.vtr`, alongside
`stats.txt` and the logs. With an explicit build directory, it goes under that
directory's `results/` instead. `--vtr` and `--no-build` may appear in either order;
`--vtr` with simdojo is rejected. Tracing is off by default and can substantially
increase output size and simulator runtime.

CTest must run on the **host**, since tests coordinate Docker containers.
Use `test.sh` instead of the Docker profile's built-in CTest run configuration.
To run a binary built by CLion, pass its build directory explicitly:

```sh
./run.sh --no-build simdojo hip-memcopy build/ide
```

Use matching images for your native host architecture (amd64 or arm64).
To override the image tags for host scripts:

```sh
SDK_IMAGE=my-sdk:local RUNNER_IMAGE=my-runner:local ./test.sh
```

Select the same SDK image in CLion's Docker toolchain. After replacing an SDK
image, rebuild before running with `--no-build`.
