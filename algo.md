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

`BallCollisionEngine::Create` creates the requested number of balls. The
default UI creates 300 balls.

Each ball begins with:

- An integer-coordinate position selected with `rand()`
- A radius from 5 through 16
- Zero velocity and acceleration

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

The default gravity mode is `Center`. It behaves like a damped spring rather
than physical gravity:

```text
target_delta = gravity_target - position
acceleration = -0.8 * velocity + 1.2 * target_delta
```

Three target modes exist:

### Normal

```text
target_delta = (0, 300)
```

This produces a constant downward acceleration of `1.2 * 300 = 360`, together
with velocity damping.

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

Every ball is compared with every other ball. Two balls overlap when:

```text
(x1 - x2)^2 + (y1 - y2)^2 <= (r1 + r2)^2
```

The squared form avoids a square root for non-colliding pairs.

There is no broad-phase acceleration structure such as a uniform grid,
quadtree, sweep-and-prune, or bounding-volume hierarchy.

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

In the current implementation, the second correction uses the already-updated
first position. This makes the result asymmetric, slightly over-separates the
balls, and causes the result to depend on traversal order.

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

The implementation does not check whether the relative normal velocity shows
that the balls are approaching. Consequently, an overlap between separating
balls can receive another impulse.

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

The current line-response implementation stores a pointer to a local fake-ball
variable and later dereferences it during dynamic collision resolution. That
pointer is already invalid, so enabling line collisions invokes undefined
behavior.

## Collision-Pair Traversal

Ball pairs are visited as ordered pairs:

```text
for each ball:
    for each target:
        if ball.id != target.id:
            test(ball, target)
```

This visits both `(A, B)` and `(B, A)`. Positional over-correction often prevents
the reverse pair from being recorded, but this is incidental. Exact contact or
multi-ball interactions can still produce duplicate or order-dependent
responses.

A conventional implementation would iterate only unique pairs:

```text
for i in [0, ball_count):
    for j in [i + 1, ball_count):
        test(ball[i], ball[j])
```

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

The division is also undefined when `intended_speed` is zero, although the
result is normally discarded before it affects the next substep.

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

OpenMP is used for:

- Per-ball acceleration and position integration
- Per-ball rendering

The integration loop is independent per ball and is safe under the current
design.

Rendering is not safely parallelized. `FillCircle` writes directly into the
shared olcPixelGameEngine pixel buffer, so overlapping circles can cause
unsynchronized writes to the same pixels.

Collision detection and response are sequential.

## Complexity

Let:

- `N` be the number of balls
- `L` be the number of line segments
- `C` be the number of recorded collisions
- `S` be the number of substeps, currently 4

The cost per frame is:

```text
Integration:             O(S * N)
Ball-ball detection:     O(S * N^2)
Ball-line detection:     O(S * N * L)
Dynamic response:        O(S * C)
```

Overall:

```text
O(S * (N^2 + N * L + C))
```

With 300 balls and four substeps, the engine performs 358,800 non-self
ball-pair tests per rendered frame. At 60 frames per second, that is about 21.5
million tests per second before rendering and collision response are counted.

The collision list reserves `N^2` entries, so its reserved memory is also
quadratic.

## Important Edge Cases

The current implementation does not safely handle:

- Two balls with exactly the same center, because normalization divides by
  zero.
- A ball centered exactly on its line contact point.
- A zero-length line segment.
- Fast objects that cross without overlapping at the end of a substep.
- Duplicate collision pairs.
- Collision impulses applied to separating objects.
- More than one screen width or height of movement in a substep.
- Collisions across screen-wrap seams.
- Valid lifetime management for fake line-collision balls.
- Stable iterative resolution of dense piles or multi-ball contacts.

## Algorithm Classification

The engine can be summarized as:

```text
Discrete-time
+ semi-implicit Euler integration
+ brute-force all-pairs overlap detection
+ positional depenetration
+ frictionless elastic normal impulses
+ fixed-count temporal substeps
```

It is suitable as a compact visual demonstration. For a larger or more robust
physics engine, the main improvements would be unique-pair traversal, guarded
normalization, approach-only impulses, stable collider ownership, an iterative
contact solver, and a spatial broad phase such as a uniform grid.
