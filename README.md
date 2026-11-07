# GPU lab samples

Two self-checking HIP examples targeting gfx942 (MI300X):

- **hip-vector-add** adds two vectors.
- **hip-memcopy** copies an integer buffer with a GPU kernel.

Both check every output element and return a nonzero status on failure.
Their sizes exercise multiple blocks and a partially filled final block.

## Requirements

Use a Linux host with a local Docker Engine, Docker CLI, CMake 3.25+, Ninja,
Bash and GNU coreutils. Your user must have permission to run Docker.

Install these existing images locally before configuring:

| Image | Purpose |
| --- | --- |
| `gem5-gpu-hsakmt-sdk:local` | ROCm 7.2.4 compilation and rocjitsu execution |
| `gem5-gpu-hsakmt-runner:local` | gem5 HSAKMT service |

Use a matched pair for your native host architecture (amd64 or arm64).
CMake checks that both images exist and have matching platforms and rocjitsu
revisions. Images are never built or pulled by this project.

You do not need a GPU, host ROCm installation or gem5 source checkout.

## Build and test

```sh
cmake --preset default
cmake --build --preset default
ctest --preset default
```

Compilation runs inside the SDK image. CTest runs both examples on both
backends, for four tests in total:

```sh
ctest --preset simdojo
ctest --preset gem5-kmd
```

**simdojo** uses rocjitsu's functional simulator in the SDK.
**gem5-kmd** connects the SDK application to the runner's gem5 timing model.
Each test has isolated containers and runtime files; the runner must exit
successfully after the application disconnects. Tests remove their containers
on completion or failure.

Executables are in `build/bin/`. Logs and gem5 statistics are in
`build/results/<backend>/<example>/`. CTest does not build executables;
rebuild after changing a source file.

To use differently tagged images:

```sh
cmake --preset default \
  -DSDK_IMAGE=my-sdk:local \
  -DRUNNER_IMAGE=my-runner:local
```

CMake resolves image IDs at configure time. Reconfigure and rebuild after
replacing an image.
