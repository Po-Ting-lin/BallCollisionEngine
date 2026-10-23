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
- Rendering and presentation remain outside this benchmark.
- Timing calls add a small fixed overhead to instrumented updates.
