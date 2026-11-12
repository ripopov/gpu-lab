# HIP Julia fractal

Run these commands from the repository root:

```sh
./run.sh simdojo hip-julia-fractal      # functional validation first
./run.sh gem5-kmd hip-julia-fractal     # timing-model validation
```

Each run writes `hip-julia-fractal.ppm` into
`build/results/<backend>/hip-julia-fractal/`. This is a binary P6 RGB image,
320×240 pixels, viewable with a PPM-capable image viewer. No assets, training,
image libraries, or preceding simulation frames are required.

The GPU evaluates `z = z² + (-0.8 + 0.156i)` with four subpixel orbits per
thread, a 192-iteration limit, and a circular orbit trap. A wave64 covers an
8×8 pixel tile. Eight-iteration batches expose independent FP32 arithmetic;
whole waves stop once all their samples escape. Escaped samples are frozen to
prevent overflow. Tile-major output makes each wave's stores contiguous and
avoids sharing output cache lines between waves. All recurrence state stays in
registers: no textures, per-iteration global loads, or intermediate buffers.

The host checks all **307,200 orbits** against an early-exiting scalar FP32
reference, including escape counts, final squared radii, and trap distances.
Explicit `fmaf` operations and disabled implicit contraction preserve identical
rounding. Smooth coloring, antialiasing, and PPM encoding happen on the host,
outside the measured GPU kernel. The test also rejects nonfinite results,
degenerate all-inside/all-outside images, and output-file errors.
