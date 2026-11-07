# GPU lab samples

Two self-checking HIP examples targeting gfx942 (MI300X):

- **hip-vector-add** adds two vectors.
- **hip-memcopy** copies an integer buffer with a GPU kernel.

Both check every output element and return a nonzero status on failure.
Their sizes exercise multiple blocks and a partially filled final block.

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

Both samples are real `add_executable` targets with their sources compiled
as HIP. CLion's Build action builds those targets in the SDK. HIP-specific
editor diagnostics still depend on CLion's language-engine support.

The shared Run configurations invoke `run.sh` on the host. It incrementally
builds the selected sample inside the SDK before launching the backend, so
there is no separate build-before-run task to configure. Use these configurations
for simulator execution; a plain CMake Application would launch the binary
directly rather than through rocjitsu.

CLion uses `build/ide`, while the host scripts use `build/cli`. Separate caches
avoid conflicts between CLion's container mount paths and the host paths used
by the scripts. Both builds use the same CMake targets and preset options.

## Build and run from the host

```sh
./build.sh
./build.sh --target hip-memcopy
./run.sh simdojo hip-memcopy
./run.sh gem5-kmd hip-vector-add
./test.sh                 # build and run all four tests
./test.sh simdojo         # build and run one backend
./test.sh gem5-kmd
```

The root CMake project uses native HIP, not Docker custom compilation commands.
Run CMake directly only in an SDK environment, or use the host scripts above.

**simdojo** uses rocjitsu's functional simulator in the SDK.
**gem5-kmd** connects the SDK application to the runner's gem5 timing model.
The host coordinates both containers; the build container needs no Docker socket.
Each test has its own containers and runtime files, and checks that the runner
exits successfully. Containers are removed on completion or failure.

Executables are in `build/cli/bin/`. Logs and gem5 statistics are in
`build/cli/results/<backend>/<example>/`. To rerun without rebuilding:

```sh
./run.sh --no-build simdojo hip-memcopy
ctest --test-dir build/cli --output-on-failure
```

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
