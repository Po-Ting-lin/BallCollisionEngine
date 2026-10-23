# Physics Throughput

This document records the CPU physics baseline and the result of replacing the
ordered all-pairs collision scan with a uniform spatial grid.

## Executive Summary

On the test machine:

- The controlled 1,000-ball case improved from **4.908 ms** to **0.345 ms per
  physics frame**, a **14.2x speedup**.
- The controlled 2,000-ball case improved from **19.028 ms** to **0.599 ms**, a
  **31.8x speedup**.
- The grid rejects **99.1%** of possible unique pairs at 1,000 balls and
  **99.6%** at 2,000 balls.
- The application-config snapshot used by this benchmark—1,000 balls,
  1920x1080, radii 1–12—takes approximately **0.400 ms per physics frame**.
- The packed active-cell follow-up reduced the 5,000-ball, radii 1–3 case from
  **3.633 ms to 0.947 ms**, a **3.84x speedup**.
- A dense 10,000-ball physics case remains around **8 ms per frame**, although
  impulse response becomes significant at that density.
- No invalid ball states were found in any benchmark or correctness test.

The grid removes the previous quadratic bottleneck for sparse and moderately
dense scenes. CPU circle rasterization and OpenGL presentation are not included,
so rendered performance must be measured separately.

## Test Environment

| Item | Value |
|---|---|
| Date | 2026-07-24 |
| CPU | AMD Ryzen 5 3500X, 6 cores / 6 logical processors |
| Memory | 16 GB |
| OS | Windows 10 Pro 64-bit, build 19045 |
| Compiler | MinGW-w64 GCC 10.3.0 |
| Build | CMake `Release` |
| OpenMP maximum | 6 threads, dynamic teams disabled |
| Simulation step | 1/60 second, split into 4 substeps |

No processor affinity, elevated priority, or background-process isolation was
used. These are practical local measurements rather than laboratory-grade
results.

## Uniform-Grid Method

Each substep now performs:

1. Integrate velocity and position.
2. Find the largest current ball radius.
3. Set grid cell size to the largest ball diameter.
4. Reset previously active cells and rebuild packed cell contents using
   ball-center positions.
5. Test pairs in the same cell.
6. Test four forward neighboring cells.
7. Correct overlaps and record collision pairs.
8. Apply collision impulses.

A cell size equal to the maximum diameter guarantees that overlapping circles
have centers in either the same cell or adjacent cells.

Only these neighbor offsets are visited:

```text
right
bottom-left
bottom
bottom-right
```

Combined with unique pairs inside each cell, this visits every relevant
candidate exactly once.

The grid is rebuilt after integration in every substep because ball positions
change continuously.

The current grid representation is a packed flat structure:

- Per-cell counts and offsets are retained across frames.
- Only counters for previously occupied cells are reset.
- Ball indices are packed contiguously by cell.
- Collision traversal visits only occupied cells and their relevant neighbors.

This avoids scanning every logical grid cell, which is especially important
when small balls produce many mostly empty cells.

## What Is Timed

The headless benchmark measures four physics phases:

1. **Integration**
   - Calculate attraction and damping.
   - Update velocity and position.
   - Wrap positions and clamp small velocities.
2. **Grid build**
   - Calculate cell dimensions.
   - Clear reusable cell buckets.
   - Insert every ball into one cell.
3. **Candidate collision processing**
   - Visit same-cell and neighboring-cell candidates.
   - Test circle overlap.
   - Correct overlapping positions.
   - Record collision pairs.
4. **Impulse response**
   - Apply elastic response to approaching collision pairs.
   - Clear collision and temporary-line-collider storage.

Rendering, OpenGL upload, window events, and presentation are excluded. The
dormant line-collider path is also excluded because benchmark scenes contain no
lines.

The interactive application separately shows a full-frame profiler in its
top-left overlay. It measures physics, screen clearing, ball drawing, and UI
drawing. `Present+` is the frame-time remainder after those phases, including
input handling, framebuffer upload, OpenGL presentation, and other pixel-engine
overhead.

## Methodology

- Positions use deterministic continuous random values.
- Controlled baseline scenes use 2048x1200 and radii 5–16, matching the
  original benchmark so before/after measurements are comparable.
- The application-config snapshot uses 1920x1080, 1,000 balls, and radii 1–12.
- Balls begin stationary in the default center-attraction mode.
- Each scene receives 10 unmeasured warm-up frames.
- Five complete benchmark executions were performed.
- Tables report the median of those five executions for each metric.

Measured work:

| Scenario | Balls | Batches | Frames per batch | Measured frames |
|:---|---:|---:|---:|---:|
| Controlled | 100 | 20 | 120 | 2,400 |
| Controlled | 300 | 5 | 120 | 600 |
| Controlled | 1,000 | 1 | 120 | 120 |
| Controlled | 2,000 | 1 | 30 | 30 |
| Grid scale | 5,000 | 1 | 30 | 30 |
| Grid scale | 10,000 | 1 | 15 | 15 |
| Small-radius grid | 5,000 | 1 | 60 | 60 |
| Current app | 1,000 | 1 | 120 | 120 |

## Before Optimization

The original detector visited every ordered pair during every substep.

| Balls | Total ms/frame | Integrate ms | All-pairs detection ms | Response us | Detection share |
|---:|---:|---:|---:|---:|---:|
| 100 | 0.193 | 0.139 | 0.054 | 0.245 | 27.9% |
| 300 | 0.583 | 0.145 | 0.437 | 0.800 | 75.0% |
| 1,000 | 4.908 | 0.167 | 4.734 | 5.473 | 96.5% |
| 2,000 | 19.028 | 0.197 | 18.833 | 7.010 | 98.9% |

Doubling from 1,000 to 2,000 balls made detection 3.98 times slower, matching
the expected `O(N^2)` growth.

## After Initial Uniform-Grid Optimization

The first uniform-grid implementation used one vector bucket per logical cell
and traversed every cell. Controlled scenes:

| Balls | Total ms/frame | Physics FPS | Integrate ms | Grid ms | Candidate ms | Response us | Candidates/frame | Pairs culled | Invalid |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 100 | 0.134 | 7,463 | 0.015 | 0.013 | 0.104 | 0.235 | 585 | 97.045% | 0 |
| 300 | 0.170 | 5,882 | 0.019 | 0.019 | 0.129 | 0.761 | 2,956 | 98.352% | 0 |
| 1,000 | 0.345 | 2,899 | 0.033 | 0.041 | 0.257 | 4.432 | 18,057 | 99.096% | 0 |
| 2,000 | 0.599 | 1,669 | 0.055 | 0.102 | 0.446 | 10.950 | 35,854 | 99.552% | 0 |

`Physics FPS` is the reciprocal of physics time only, not rendered frame rate.

### Before/After Comparison

| Balls | Before ms/frame | Grid ms/frame | Speedup |
|---:|---:|---:|---:|
| 100 | 0.193 | 0.134 | 1.44x |
| 300 | 0.583 | 0.170 | 3.43x |
| 1,000 | 4.908 | 0.345 | 14.2x |
| 2,000 | 19.028 | 0.599 | 31.8x |

The speedup grows with ball count because the old quadratic work rises much
faster than grid construction and local candidate processing.

At very small counts, scanning and clearing grid cells is a meaningful fixed
cost. A direct unique-pair loop can still be competitive for tiny scenes, but
the current application starts well beyond that range.

## Application Configuration Benchmark Snapshot

The settings at the time this benchmark was recorded were:

```text
Resolution: 1920 x 1080 fullscreen
Balls:      1,000
Radius:     1 through 12
```

Measured result:

| Total ms/frame | Physics FPS | Integrate ms | Grid ms | Candidate ms | Response us | Candidates/frame | Pairs culled | Contacts/frame | Applied impulses/frame |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 0.400 | 2,500 | 0.034 | 0.049 | 0.308 | 3.993 | 18,093 | 99.094% | 431 | 413 |

Physics consumes approximately **2.4%** of a 16.67 ms frame budget in this
configuration. Rendering is likely to be more important than physics now.

## Larger Grid Cases

These cases use the controlled 2048x1200 scene and radii 5–16:

| Balls | Total ms/frame | Physics FPS | Integrate ms | Grid ms | Candidate ms | Response ms | Candidates/frame | Pairs culled | Contacts/frame | Applied impulses/frame |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 5,000 | 2.338 | 428 | 0.190 | 0.151 | 1.709 | 0.263 | 187,405 | 99.625% | 22,649 | 18,936 |
| 10,000 | 7.985 | 125 | 0.218 | 0.268 | 5.574 | 1.930 | 719,005 | 99.640% | 117,477 | 60,815 |

At 10,000 balls the physics pipeline still fits inside a 60 FPS budget.
However, this is a densely overlapping scene, so impulse response grows to
about 1.9 ms and CPU rendering of 10,000 circles would be substantial.

## Active-Cell Follow-Up

A targeted benchmark was added for the current small-radius stress shape:
5,000 balls, 1920x1080, and radii 1–3. The maximum diameter is 6 pixels, so the
logical grid contains 57,600 cells and the four substeps previously traversed
230,400 cell positions per frame.

Median of five executions:

| Grid representation | Total ms/frame | Physics FPS | Integrate ms | Grid build ms | Candidate ms | Response us | Candidates/frame |
|:---|---:|---:|---:|---:|---:|---:|---:|
| Vector bucket, scan every cell | 3.633 | 275 | 0.192 | 0.546 | 2.877 | 3.120 | 9,228 |
| Packed flat, active cells only | 0.947 | 1,056 | 0.188 | 0.262 | 0.487 | 4.117 | 9,199 |

The packed active-cell grid is **3.84x faster overall** in this case. Grid
construction is **2.08x faster**, and candidate traversal is **5.91x faster**.
The small candidate-count difference comes from deterministic but different
cell traversal order changing the evolving contact layout slightly.

## OpenMP Behavior

Before the grid change, six-thread integration was slower than one thread for
all tested sizes:

| Balls | One-thread integration ms | Six-thread integration ms |
|---:|---:|---:|
| 100 | 0.014 | 0.139 |
| 300 | 0.019 | 0.145 |
| 1,000 | 0.036 | 0.167 |
| 2,000 | 0.066 | 0.197 |

The integration loop was too small to amortize four OpenMP dispatches and
barriers per frame.

The engine now executes integration serially below 4,096 balls and enables the
OpenMP loop only at or above that threshold. The 1,000-ball application
snapshot therefore avoids OpenMP overhead automatically.

## Correctness Improvements Included

The grid refactor also addresses several issues that affected dense scenes:

- Candidate pairs are unique rather than ordered duplicates.
- Position correction uses a symmetric contact normal.
- Coincident ball centers are separated deterministically instead of dividing
  by zero.
- Zero-speed remaining-time calculations are guarded.
- Impulses are applied only when bodies are approaching.
- Temporary line-collider balls now have stable storage.
- Degenerate zero-length lines are ignored safely.
- The collision list no longer reserves quadratic memory.

Automated tests cover:

- Sparse-grid pair culling.
- Collisions across cell boundaries.
- Collisions across diagonal neighboring cells.
- Unique candidate traversal.
- Coincident-center stability.

## Reproducing Results

Build, test, and run the benchmark:

```powershell
.\build.ps1 -Configuration Release -Test -Benchmark
```

Run only the interactive application:

```powershell
.\build.ps1 -Run
```

The benchmark source is `BallCollision/Benchmark.cpp`. Phase timing is enabled
only when a `PhysicsMetrics` object is passed; the interactive update path does
not call the timing clock.

## Dead-Work Removal and Rendering Follow-Up

The per-ball remaining-time estimate cost two square roots per ball per
substep, but its result was overwritten before it was ever used. It was
removed together with the `sx`, `sy`, and `remainingTime` fields, shrinking
`Ball` from 52 to 40 bytes. Contact counts are unchanged, so behavior is
identical.

Physics medians on a 2-core Linux VM with GCC 11.4 (single run, so treat as
indicative):

| Scenario | Balls | Before ms/frame | After ms/frame |
|:---|---:|---:|---:|
| Controlled | 2,000 | 0.567 | 0.538 |
| Grid scale | 5,000 | 2.527 | 2.307 |
| Small-radius grid | 5,000 | 0.899 | 0.781 |

Rendering changes, measured with a standalone 1920x1080 replica of the
pixel-engine draw path (8,000 balls, radii 2–3):

| Step | Before | After |
|:---|---:|---:|
| Screen clear | `FillRect`, ~8.2 ms | `Clear()`, ~0.1 ms |
| Ball drawing | `FillCircle`, ~1.1 ms | `_fillCircleFast`, ~0.3 ms |

`FillRect` called `Draw()` for every pixel in column-major order, which is
both branch-heavy and cache-hostile. The in-app profiler is the reference
for real numbers on the target machine.

## GPU Rendering

Rendering later moved to the GPU entirely. The CPU now only fills a vertex
array (four vertices per ball) and the pixel engine's per-frame 1920x1080
framebuffer upload is skipped by replacing layer 0 with a custom OpenGL render
function. Both the screen clear and per-pixel circle rasterization are gone
from the CPU. Measure the result with the in-app profiler: `Balls` is the
vertex-array build and the GPU work appears in `Present+`.

## Full-Frame Benchmark

`BallCollisionBenchmark` times physics only. The full-frame benchmark runs the
real application, window and OpenGL renderer included:

```powershell
.\build.ps1 -FrameBenchmark
```

### Method

- `BallCollision.exe --frame-benchmark [output.csv]` plays a fixed scene list
  in one 1920x1080 full-screen window. The engine is recreated for each scene,
  and `srand` is seeded per scene, so the random layouts repeat.
- Scenes: 2,000, 8,000, and 20,000 balls with radii 2-3, `random` pattern,
  `center` gravity (strength 1.2, drag 0.8); the 8,000-ball scene again inside
  an octagon of eight walls, which exercises the wall-collision path; then the
  scene from `config.ini`.
- Each scene runs 60 warm-up frames, then 600 measured frames.
- The simulation always advances by 1/60 s per frame, independent of the real
  frame time, so every run simulates the same states.
- Mouse and keyboard input are ignored, except `Esc` to abort.
- V-sync is off (`Construct` is called without the `vsync` argument).
- Per frame it records:
  - `Physics`: `BallCollisionEngine::Update`.
  - `Balls`: building the ball vertex array.
  - `UI`: wall geometry, helper lines, and the HUD texture update.
  - `Render (GPU)`: the layer-0 render function, ending in `glFinish()`, so it
    covers draw submission plus GPU execution.
  - `Present+`: the frame time left after the four phases above (buffer swap,
    window events, and pixel-engine overhead).
  - `Frame`: the pixel engine's `fElapsedTime` for that frame.
  - Heap allocations: global `operator new` calls during the frame. The
    counting `operator new` is in `BallCollision/AllocationCounter.cpp` and is
    linked only into `BallCollision.exe`. Allocations made by the OpenGL driver
    or Win32 are not counted.
- For each phase it reports average, median, P95, P99 (nearest rank), and
  maximum, plus average FPS (1 / average frame time).

`glFinish()` stalls the CPU until the GPU is idle, so CPU and GPU never
overlap in this mode. That makes the phase split readable, but the FPS is a
little lower than the overlapped interactive loop could reach.

### Results

Measured 2026-10-23 on the machine in [Test Environment](#test-environment)
(Ryzen 5 3500X), NVIDIA GeForce RTX 3070 Ti (driver 32.0.15.6094),
WinLibs MinGW-w64 GCC 16.2.0 UCRT, Release, statically linked runtime.
`config.ini` at the time: 8,000 balls, radii 2-3, `rectangle_cluster`,
gravity `none`, drag 0.6.

| Scene | Balls | Avg FPS | Frame avg ms | Frame p99 ms | Physics avg ms | Balls avg ms | UI avg ms | Render (GPU) avg ms | Render p99 ms | Present+ avg ms | Allocs/frame avg | Allocs/frame max |
|:---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| random 2k center | 2,000 | 1104.2 | 0.906 | 1.182 | 0.549 | 0.095 | 0.058 | 0.171 | 0.272 | 0.033 | 0.000 | 0 |
| random 8k center | 8,000 | 236.7 | 4.225 | 6.186 | 3.310 | 0.397 | 0.062 | 0.434 | 1.141 | 0.022 | 0.005 | 1 |
| random 20k center | 20,000 | 69.5 | 14.385 | 17.241 | 11.602 | 0.832 | 0.067 | 1.857 | 2.144 | 0.028 | 0.015 | 1 |
| random 8k center walls | 8,000 | 160.6 | 6.226 | 7.482 | 4.539 | 0.401 | 0.063 | 1.199 | 1.847 | 0.023 | 0.013 | 2 |
| config.ini | 8,000 | 251.5 | 3.976 | 5.671 | 2.147 | 0.358 | 0.057 | 1.391 | 3.119 | 0.023 | 0.005 | 1 |

Repeated runs of the same build varied by up to 15%. The `config.ini` scene
varied most, between about 220 and 330 FPS, mostly in `Render (GPU)`.

### Per-Frame Heap Allocations

The first version of the frame loop allocated on the heap every frame:

| Source | Allocations per frame | Fix |
|:---|---:|:---|
| HUD text: `std::ostringstream` and string concatenations longer than the small-string buffer | 7 | `snprintf` into a stack buffer, assigned into one reused `std::string` |
| Wall contacts: `_fakeBalls` was a `std::deque`, which frees and reallocates blocks every substep | about 90 with 8 walls | `std::vector` storage that keeps its capacity; pointers to the stand-in balls are taken after the wall pass, when the vector no longer grows |
| `_lineVertices` had no reserve, and walls could grow `_quadVertices` past its ball-only reserve | only while growing | reserve both up front, with headroom for walls |
| HUD texture: `Decal::Update()` calls `glTexImage2D` every frame, which can make the driver reallocate the texture (not counted above) | GPU side | `glTexSubImage2D` into the existing texture |

| Scene | Allocs/frame before | Allocs/frame after |
|:---|---:|---:|
| random 2k center | 7.000 | 0.000 |
| random 8k center | 7.005 | 0.005 |
| random 20k center | 7.015 | 0.015 |
| random 8k center walls | 97.347 (max 137) | 0.013 (max 2) |
| config.ini | 6.005 | 0.005 |

The remaining single allocation, about once per second, is the pixel engine
building its window-title string for the FPS counter. The rare 2 in the wall
scene is a contact vector growing past its previous peak, which stops once
the peak contact count is reached.

Observations:

- Physics dominates every scene: 54-81% of the frame. At 20,000 balls it alone
  takes 11.6 ms, so the frame budget is set by the CPU solver, not rendering.
- Eight walls cost about 1.2 ms of physics at 8,000 balls, because every ball
  is tested against every wall with no spatial culling.
- GPU rendering stays under 2 ms even at 20,000 balls. Building the vertex
  array costs about 0.04 us per ball.
- `Present+` is under 0.1 ms on average once GPU time is separated. In the
  interactive HUD, `Present+` also includes this GPU time.
- The 20,000-ball scene holds 60 FPS on average, but its P99 frame time
  (17.2 ms) misses a 60 Hz budget.

## Physics Optimization Series

Each step is a separate commit. Times are physics milliseconds per frame, the
median of three runs on the machine above (GCC 16.2, Release). `Headless 10k`
is the total of the `grid scale` 10,000-ball case in `BallCollisionBenchmark`;
the other columns are `Physics` averages from `-FrameBenchmark`.

| Step | Headless 10k | 2k center | 8k center | 20k center | 8k center walls | config.ini 8k |
|:---|---:|---:|---:|---:|---:|---:|
| Before | 8.565 | 0.557 | 3.284 | 11.546 | 4.490 | 2.059 |
| 1. Cell-ordered working copy | 7.854 | 0.513 | 3.006 | 10.169 | 4.088 | 2.072 |
| 2. Impulse fused into pair test | 6.376 | 0.465 | 2.582 | 8.394 | 4.113 | 1.718 |

Notes per step:

1. **Cell-ordered working copy.** The narrow phase used to read 40-byte `Ball`
   records through an index array, in spawn order, which is close to random
   memory access for the `random` pattern. Each substep now copies the hot
   fields into 28-byte records packed in row-major cell order, runs the pair
   tests, impulses, and boundary clamping on them, and copies positions and
   velocities back. With cells kept in first-appearance order, results were
   bit-identical and the gain was about 10% at 20,000 balls. Row-major order
   added roughly another 10% on the 8,000-ball scenes, but changes the pair
   order, so results differ slightly. `config.ini` spawns balls row by row,
   so it already had good locality and does not change. Steady-state frames
   still make no heap allocations.
2. **Impulse fused into the pair test.** Positional correction used to record
   every contact in a collision list (about 120,000 entries per frame at
   10,000 balls), and a second pass recomputed each distance and square root
   to apply the impulse. The impulse is now applied right after the
   correction, with the same normal and the same approach-only rule, and wall
   contacts bounce immediately instead of through stored stand-in balls. The
   headless response phase drops from 2.15 ms to 0.14 ms at 10,000 balls.
   Impulses are applied in a different order, so results are not
   bit-identical, but contact counts stay within 1% and no invalid states
   appear. The wall scene does not improve, because testing every ball
   against every wall dominates it.

## Next Optimization Priorities

1. Record the interactive full-frame profiler values for physics, ball drawing,
   and `Present+` under the target scene.
2. Use GPU-instanced circle rendering if ball drawing or presentation dominates.
3. Tune cell size for known radius distributions.
4. Consider parallel candidate processing only with a race-safe accumulated
   correction scheme.
5. Consider GPU compute and instanced rendering if the target remains tens of
   thousands of balls.

## Limitations

- Results are specific to this machine and compiler.
- Collision density changes as center attraction evolves.
- Before/after physics behavior is not bit-identical because the optimized
  solver uses unique pairs and approach-only impulses.
- The line-collision path is fixed but not included in throughput scenes.
- Rendering and presentation remain outside the physics benchmark; see
  [Full-Frame Benchmark](#full-frame-benchmark).
- Timing calls add a small fixed overhead to instrumented updates.
