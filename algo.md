# Ball Collision Engine Algorithm

This repository implements a small 2D ball-physics visualizer using
[olcPixelGameEngine](BallCollision/olcPixelGameEngine.h). The simulation uses
discrete time steps, circle-overlap detection, positional correction, and
impulse-based velocity response.

## Repository Structure

- `BallCollision/Source.cpp` contains the program entry point.
- `BallCollision/UI_mainConsole.h` creates the window, handles mouse input, and
  renders the simulation.
- `BallCollision/BallCollisionEngine.h` declares the physics engine.
- `BallCollision/BallCollisionEngine.cpp` implements integration and collision
  handling.
- `BallCollision/objectStructure.h` defines the `Ball` and `Line` data
  structures.

The main execution path is:

```text
main
  -> RunBallCollision
    -> BallCollision::OnUserCreate
      -> BallCollisionEngine::Create
    -> BallCollision::OnUserUpdate
      -> process mouse input
      -> BallCollisionEngine::Update
      -> render balls and lines
```

## Simulation State

Each ball stores:

- Current position: `(x, y)`
- Position at the beginning of the substep: `(sx, sy)`
- Velocity: `(vx, vy)`
- Acceleration: `(ax, ay)`
- Radius: `r`
- Mass: `10 * r`
- Remaining substep time

The mass model is an artistic choice. For a physically uniform 2D disk, mass
would normally be proportional to `r^2`.

## Initialization

`BallCollisionEngine::Create` creates the number of balls requested by
`num_balls` in `config.ini`.

Each ball begins with:

- An integer-coordinate position selected with `rand()`
- An integer radius selected from the inclusive `ball_radius_min` through
  `ball_radius_max` configuration range
- Zero velocity and acceleration

`pattern_mode` selects the initial position distribution:

- `random`: positions across the complete screen, preserving the original
  behavior.
- `circle_cluster`: a centered circular formation selected from the nearest
  points of a hexagonal close-packed lattice.
- `rectangle_cluster`: centered tangent rows arranged into a rectangle matching
  the screen aspect ratio.

Both cluster modes set every ball radius to `ball_radius_min` and place adjacent
centers exactly `2 * ball_radius_min` apart. If the requested packed formation
cannot fit on screen, startup reports a configuration error instead of placing
balls outside the simulation area.

No random seed is set, so the initial sequence is normally repeatable between
program launches using the same C runtime.

## Frame Update and Substeps

Each rendered frame is divided into four physics substeps:

```text
substep_dt = frame_dt / 4

repeat 4 times:
    set each ball's remaining time to substep_dt
    integrate acceleration, velocity, and position
    detect overlaps and correct positions
    resolve collision velocities
```

Substeps reduce numerical error and make tunneling less likely, but collision
detection is still discrete. A sufficiently fast ball can move through another
ball between two checks.

## Acceleration Model

The gravity mode is selected with `gravity` in `config.ini`.
`gravity_strength` controls the acceleration multiplier and defaults to `1.2`.
Four modes exist. The modes other than `None` use velocity damping and, for the
attraction modes, behave like damped springs rather than physical gravity:

At runtime, pressing `G` cycles through `None`, `Normal`, `Center`, and
`Bicenter`, then returns to `None`.

```text
target_delta = gravity_target - position
acceleration = -velocity_drag * velocity
             + gravity_strength * target_delta
```

### None

```text
acceleration = (0, 0)
```

This mode disables gravitational acceleration. Set `velocity_drag = 0` as well
for space-like motion where a ball retains its velocity until it collides with
another ball or a boundary.

### Normal

```text
target_delta = (0, 300)
```

With the default strength, this produces a constant downward acceleration of
`1.2 * 300 = 360`, together with velocity damping.

### Center

```text
gravity_target = (screen_width / 2, screen_height / 2)
```

All balls are attracted to the center of the screen.

### Bicenter

Balls in the left half are attracted to:

```text
(screen_width * 0.25, screen_height * 0.5)
```

Balls in the right half are attracted to:

```text
(screen_width * 0.75, screen_height * 0.5)
```

## Numerical Integration

The engine uses semi-implicit Euler integration. For substep duration `dt`:

```text
velocity = velocity + acceleration * dt
position = position + velocity * dt
```

Updating position with the new velocity is generally more stable for simple
mechanical simulations than explicit Euler integration.

After integration, the center of each ball wraps around the screen:

```text
if x < 0:             x += screen_width
if x >= screen_width: x -= screen_width
if y < 0:             y += screen_height
if y >= screen_height:y -= screen_height
```

Only one width or height is added or subtracted. Extremely large movement in a
single substep can therefore leave a ball outside the screen. The collision
algorithm also does not use wrapped distance, so balls on opposite sides of a
screen seam do not collide with each other.

Very small squared speeds are clamped to zero:

```text
if vx^2 + vy^2 < 0.1:
    velocity = (0, 0)
```

## Ball-Ball Collision Detection

The engine rebuilds a uniform grid after integration in every substep. Grid
cell size is set to the largest current ball diameter. Each ball is inserted
into the cell containing its center.

The grid uses packed flat arrays. Rebuilding it resets counters only for cells
that were active in the previous substep, counts the new occupants, and packs
ball indices into one contiguous array. Collision traversal iterates only the
active-cell list, so empty cells are neither cleared nor scanned.

Pairs are tested within the same cell and across four forward neighbors:

```text
right, bottom-left, bottom, bottom-right
```

This covers all eight neighboring directions while emitting each candidate
pair only once. Cell size is at least the maximum collision diameter, so an
overlapping pair cannot be separated by more than one cell on either axis.

Candidate balls overlap when:

```text
(x1 - x2)^2 + (y1 - y2)^2 <= (r1 + r2)^2
```

The squared form avoids a square root for non-colliding pairs.

The grid is the broad phase; the squared circle test is the narrow phase.

## Positional Correction

When two balls overlap, the engine first moves their centers apart. If:

```text
distance = length(position1 - position2)
penetration = radius1 + radius2 - distance
normal = (position1 - position2) / distance
```

the intended correction is approximately:

```text
position1 += 0.5 * penetration * normal
position2 -= 0.5 * penetration * normal
```

This prevents balls from remaining visibly embedded before the velocity
response is calculated.

The correction uses one contact normal and moves both balls symmetrically.
Coincident centers use a deterministic horizontal normal to avoid division by
zero.

## Dynamic Collision Response

After positional correction, each recorded collision is resolved as a
perfectly elastic collision along the contact normal.

Let:

```text
n = normalize(position1 - position2)
relative_normal_velocity = dot(velocity1 - velocity2, n)
p = 2 * relative_normal_velocity / (mass1 + mass2)
```

The velocities are updated with:

```text
velocity1 -= p * mass2 * n
velocity2 += p * mass1 * n
```

This is the closed-form impulse equation for two frictionless, perfectly
elastic bodies:

- Momentum along the collision normal is conserved.
- Kinetic energy is conserved when the impulse is applied once to an
  approaching pair.
- Tangential velocity is unchanged.
- There is no rotational motion or contact friction.

The impulse is applied only when relative normal velocity shows that the
bodies are approaching.

## Line Collision Model

A line is treated as a thick line segment, equivalent to a capsule.

For each ball:

1. Project the ball center onto the line.
2. Clamp the projection to the segment endpoints.
3. Find the closest point on the segment.
4. Test whether the distance to that point is at most `ball.r + line.r`.
5. Correct the ball's position away from the line.
6. Approximate a bounce by colliding the ball with a temporary fake ball at the
   closest point.

The fake ball is assigned:

```text
radius = line.radius
mass = ball.mass * 0.8
velocity = -ball.velocity
```

Line creation is commented out in the default scene.

Temporary fake balls use stable `std::deque` storage until impulse resolution.
Zero-length lines are ignored, and a ball centered exactly on its closest line
point receives a perpendicular fallback normal.

## Collision-Pair Traversal

Pairs in the same grid cell use:

```text
for i in [0, cell_size):
    for j in [i + 1, cell_size):
        test(cell[i], cell[j])
```

Pairs across the four selected neighbor directions use the Cartesian product
of the two cell buckets. No pair is intentionally emitted twice.

## Remaining-Time Calculation

After collision detection, the engine estimates how much of a ball's substep
was consumed:

```text
intended_speed = length(velocity)
actual_distance = length(position - substep_start_position)
actual_time = actual_distance / intended_speed
remaining_time -= actual_time
```

However, the engine does not perform another integration or collision pass
using this remaining time. The value is reset at the next substep, so this is
currently an incomplete continuous-collision mechanism.

Zero intended speed is guarded and sets remaining time to zero.

## User Interaction

- Hold the left mouse button on a ball to drag it.
- Press and release the right mouse button on a ball to launch it.
- The launch velocity points away from the release position:

```text
velocity = 20 * (ball_position - mouse_release_position)
```

Ball color is based on speed: slower balls are cyan-like and faster balls
become red.

## Parallelism

OpenMP is used for per-ball acceleration and position integration only when
there are at least 4,096 balls. Below that threshold, the loop runs serially
because measured OpenMP dispatch and barrier overhead exceeds the integration
work.

Rendering is sequential because `FillCircle` writes into a shared
olcPixelGameEngine pixel buffer.

Collision detection and response are sequential.

## Complexity

Let:

- `N` be the number of balls
- `L` be the number of line segments
- `G` be the number of grid cells
- `P` be the number of grid-generated candidate pairs
- `C` be the number of recorded collisions
- `S` be the number of substeps, currently 4

The cost per frame is:

```text
Integration:             O(S * N)
Grid rebuild:            O(S * (N + G))
Ball-ball detection:     O(S * P)
Ball-line detection:     O(S * N * L)
Dynamic response:        O(S * C)
```

Typical overall cost:

```text
O(S * (N + G + P + N * L + C))
```

For a reasonably even distribution with bounded local density, `P` grows near
linearly with `N`. The worst case remains `O(N^2)` if most balls occupy one cell
or its immediate neighbors.

Grid storage is `O(N + G)`. The collision list initially reserves `8 * N`
entries and grows only if actual contacts require more.

Measured before/after results are recorded in [throughput.md](throughput.md).

## Important Edge Cases

Important remaining limitations are:

- Fast objects that cross without overlapping at the end of a substep.
- More than one screen width or height of movement in a substep.
- Collisions across screen-wrap seams.
- Stable iterative resolution of dense piles or multi-ball contacts.
- Contacts created by positional correction after the grid is built may wait
  until the next substep.

## Algorithm Classification

The engine can be summarized as:

```text
Discrete-time
+ semi-implicit Euler integration
+ uniform-grid broad phase
+ unique local-pair overlap detection
+ positional depenetration
+ frictionless elastic normal impulses
+ fixed-count temporal substeps
```

It is suitable as a compact visual demonstration. A more robust physics engine
would add swept collision detection, an iterative contact solver, consistent
toroidal or bounded-edge semantics, and rotational/frictional response.
