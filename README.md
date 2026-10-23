# Ball Collision Engine

A 2D ball-collision visualizer built with C++17 and
[olcPixelGameEngine](BallCollision/olcPixelGameEngine.h).

The physics implementation is documented in [algo.md](algo.md).

## Requirements

The PowerShell build uses a MinGW-w64 toolchain and does not require a Visual
Studio solution or MSBuild.

Install these tools and make sure they are available on `PATH`:

- CMake 3.15 or newer
- MinGW-w64 with GCC 9 or newer and `g++.exe`
- `mingw32-make.exe`

The compiler must include OpenMP support.

If multiple MinGW-w64 installations are on `PATH`, the script selects the
compiler with the highest reported GCC version.

## Build

From PowerShell in the repository root:

```powershell
.\build.ps1
```

The default configuration is `Release`. The executable is written to:

```text
build/bin/BallCollision.exe
```

Build and launch the program:

```powershell
.\build.ps1 -Run
```

Create a debug build:

```powershell
.\build.ps1 -Configuration Debug
```

Delete the existing CMake build directory before building:

```powershell
.\build.ps1 -Clean
```

Options can be combined:

```powershell
.\build.ps1 -Configuration Debug -Clean -Run
```

Select a specific MinGW-w64 installation:

```powershell
.\build.ps1 -ToolchainDirectory C:\msys64\mingw64\bin -Clean
```

Run the headless physics throughput benchmark:

```powershell
.\build.ps1 -Benchmark
```

Run the full-frame benchmark of the real application, including GPU rendering:

```powershell
.\build.ps1 -FrameBenchmark
```

This builds Release and runs `BallCollision.exe --frame-benchmark
build\frame_benchmark.csv`. The app opens its normal window and plays five
fixed scenes back to back (2,000 / 8,000 / 20,000 random balls with center
gravity, 8,000 balls inside eight walls, then the `config.ini` scene) with a
fixed 1/60 s simulation step and input disabled. It prints a Markdown table per
scene to the console, including heap allocations per frame, writes one CSV row
per scene, and exits by itself. The executable can also be run
directly: `BallCollision.exe --frame-benchmark [output.csv]`.

Build and run the automated physics tests:

```powershell
.\build.ps1 -Test
```

Benchmark methodology and recorded results are documented in
[`throughput.md`](throughput.md).

MinGW builds link the GCC runtime statically (`-static`), so the executables do
not load `libstdc++-6.dll`, `libgomp-1.dll`, or `libwinpthread-1.dll` from an
older MinGW that may be earlier on `PATH`.

## Runtime Configuration

Edit [`config.ini`](config.ini) to change the initial scene without recompiling:

```ini
ball_radius_min = 2
ball_radius_max = 5
pattern_mode = random
right_drag_launch_speed = 60.0
gravity = none
gravity_strength = 1.2
velocity_drag = 0.8
num_balls = 3000
mouse_force_strength = 4000.0
mouse_force_radius = 200.0
wall_radius = 4.0
```

Radius bounds are inclusive. Set `ball_radius_min` and `ball_radius_max` to the
same value for fixed-size balls. Launch speed multiplies the distance between
the selected ball and the mouse cursor when the right button is released.
`pattern_mode` accepts `random`, `circle_cluster`, or `rectangle_cluster`.
`circle_cluster` uses centered hexagonal close packing, while
`rectangle_cluster` uses centered tangent rows. Both cluster modes give every
ball the `ball_radius_min` radius; `ball_radius_max` applies only to `random`.
Gravity accepts `none`, `normal`, `center`, or `bicenter`; values are
case-insensitive and selects the initial mode. `gravity_strength` scales the
acceleration of the other three modes; `0` disables their gravitational force,
while `1.2` preserves the original behavior. `velocity_drag` controls velocity
damping independently of gravity. Set it to `0` for space-like coasting.
`mouse_force_strength` is the `Q` / `E` acceleration at the cursor, fading to
zero at `mouse_force_radius` pixels. `wall_radius` sets the thickness of drawn
walls.

The build copies the configuration file beside the executable. Running through
`.\build.ps1 -Run` reads the root copy shown above.

## Full-Frame Profiler

The top-left overlay reports smoothed frame time and these CPU phases:

- Physics simulation
- Ball geometry: building the vertex array for every ball
- Lines and UI: wall geometry, helper lines, and the HUD text texture
- `Present+`, the remaining frame time after the three measured phases

Everything is drawn on the GPU. A custom render function on layer 0 sends all
balls and walls to OpenGL as one batch of textured quads, so the 1920x1080 CPU
framebuffer is no longer uploaded every frame. `Present+` therefore covers
pixel-engine input handling, the OpenGL draw calls, buffer presentation, and
other small frame-loop costs. The frame benchmark (`-FrameBenchmark`) splits
the GPU draw time out of `Present+` by calling `glFinish()` at the end of the
render function and timing it.

## Manual CMake Build

The script runs the equivalent of:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The root `CMakeLists.txt` also contains the required Windows system libraries,
OpenMP configuration, and Linux dependencies.

## Controls

- Press `Esc` to exit the simulation.
- Press `G` to cycle `none → normal → center → bicenter → none`.
- Hold the left mouse button on a ball to drag it.
- Press and release the right mouse button on a ball to launch it away from the
  mouse cursor.
- Right-drag on empty space to draw a wall. Middle-click a wall to delete it,
  or press `C` to clear every wall. Left-drag a wall end to move it.
- Hold `Q` to pull nearby balls toward the cursor, or `E` to push them away.
- Press `P` or `Space` to pause, `-` / `=` to halve or double the simulation
  speed (1/16x to 2x), and `0` to return to 1x.
- Press `R` to respawn the balls. Walls are kept.

## Third-Party Component

The repository includes olcPixelGameEngine by OneLoneCoder. Its license and
documentation are contained in
[`BallCollision/olcPixelGameEngine.h`](BallCollision/olcPixelGameEngine.h).
