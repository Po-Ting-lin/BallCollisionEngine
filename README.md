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

## Manual CMake Build

The script runs the equivalent of:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The root `CMakeLists.txt` also contains the required Windows system libraries,
OpenMP configuration, and Linux dependencies.

## Controls

- Hold the left mouse button on a ball to drag it.
- Press and release the right mouse button on a ball to launch it away from the
  mouse cursor.

## Third-Party Component

The repository includes olcPixelGameEngine by OneLoneCoder. Its license and
documentation are contained in
[`BallCollision/olcPixelGameEngine.h`](BallCollision/olcPixelGameEngine.h).
