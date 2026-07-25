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

Build and run the automated physics tests:

```powershell
.\build.ps1 -Test
```

Benchmark methodology and recorded results are documented in
[`throughput.md`](throughput.md).

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

The build copies the configuration file beside the executable. Running through
`.\build.ps1 -Run` reads the root copy shown above.

## Full-Frame Profiler

The top-left overlay reports smoothed frame time and these CPU phases:

- Physics simulation
- Full-screen clear
- Ball rasterization
- Lines and UI drawing
- `Present+`, the remaining frame time after the four measured phases

`Present+` includes pixel-engine input handling, framebuffer upload, OpenGL
presentation, and other small frame-loop costs. Measurements are smoothed for
readability.

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

## Third-Party Component

The repository includes olcPixelGameEngine by OneLoneCoder. Its license and
documentation are contained in
[`BallCollision/olcPixelGameEngine.h`](BallCollision/olcPixelGameEngine.h).
