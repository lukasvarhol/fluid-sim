# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

**Standard build (Release):**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target fluid-sim
```

**Windows with vcpkg:**
```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --target fluid-sim
```

**With benchmarks:**
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build --target fluid-sim-benchmarks
```

**Run benchmarks:**
```powershell
.\benchmarks\run_benchmarks.ps1   # Windows
# or
./benchmarks/run_benchmarks.sh    # Unix
```

**Requirements:** CMake 3.10+, C++20 compiler, GLFW3, OpenGL. On macOS, TBB must be installed separately (replaces `std::execution::par_unseq`).

## Architecture

This is a real-time 2D Position-Based Fluids (PBF) simulation (Macklin & Müller 2013) built in C++20/OpenGL, targeting ~7,000 particles at interactive frame rates on CPU. A physics interaction subsystem layered on top enables colliders, containers, triggers, and levers.

### Simulation Core (`src/particles.h/.cpp`)

The `Particles` class owns all simulation state and runs the PBF loop each frame:
1. **Spatial hashing** — flat grid of `Cell` objects for O(kN) neighbor search
2. **Neighbor list construction** — fixed-size flat arrays (256 max neighbors/particle) for cache locality
3. **Verlet acceleration** — skips grid/neighbor rebuilds when particles haven't moved beyond a skin radius threshold
4. **PBF constraint solver** — iterative density/pressure correction (`NUM_ITERATIONS` times)
5. **XSPH viscosity + vorticity confinement** — smooth velocities and add turbulence
6. **Boundary clamping** — keep particles in-bounds

All per-particle loops use `std::execution::par_unseq` (TBB fallback on macOS).

### Physics Interaction System (`src/physics/`)

A post-solve layer applied after the PBF solver each frame. It is scene-configuration driven: objects are pushed into `InteractionSystem` vectors at startup and updated every tick.

**`collider.h/.cpp`** — Two collider types resolved against every particle position/velocity after the solver:
- `PlaneCollider`: half-space defined by a point and outward normal, with restitution.
- `AABBCollider`: solid axis-aligned box; finds the minimum-penetration face and snaps the particle out with velocity response.

**`container.h/.cpp`** — An AABB region (`Container`) that counts how many particles are currently inside it and computes `totalMass` and `fillRatio` each frame. Purely observational — does not push particles.

**`trigger.h/.cpp`** — Evaluates a threshold condition against a container metric (`ParticleCount`, `TotalMass`, or `FillRatio`) and sets an `active` flag. Two modes:
- `OneShot` — fires once when the threshold is crossed; `active` remains false on subsequent frames.
- `Persistent` — `active` tracks whether the condition is currently met.

**`lever.h/.cpp`** — A 2D angular seesaw with a pivot, arm length, and angular dynamics (torque, damping, restore stiffness). `updateLever` integrates angle and angular velocity each frame. `leverEndA`/`leverEndB` return the two arm tips in world space.

**`interaction_system.h/.cpp`** — Aggregates all of the above and drives the update order each frame:
1. Resolve all plane and AABB colliders against every particle.
2. Update all containers (particle count / fill ratio).
3. Evaluate all triggers against their bound container metric.
4. Drive all levers via their `LeverSource` (container `fillRatio` or trigger `active`).

`InteractionSystem::renderDebugUI()` exposes live container, trigger, and lever state as an ImGui collapsing panel. `InteractionSystem::reset()` resets all runtime state without touching configured objects.

### Runtime Update Order

Each frame in the main loop:
1. `particles.update(dt, ...)` — full PBF solve; produces updated `positions` and `velocities`.
2. `interactionSystem.update(dt, positions, velocities)` — post-solve pass: collider resolution → container census → trigger evaluation → lever integration.
3. Particle positions and colors are uploaded to the GPU and drawn with one instanced draw call.
4. If debug visualization is enabled, `DebugRenderer` draws physics object outlines.
5. ImGui renders the HUD with live state from both systems.

### Reset Semantics

Pressing `R` calls `reset(particles)` and `interactionSystem.reset()`. After reset:
- Particle positions and velocities return to their initial grid layout.
- All container runtime values (`particleCount`, `totalMass`, `fillRatio`) clear to zero.
- All trigger runtime state clears (`active = false`, `firedOnce = false`).
- All lever runtime state resets (`angle = 0`, `angularVel = 0`).
- Configured objects (colliders, containers, triggers, levers, leverSources) are **not** removed.

### Rendering (`src/triangle_mesh.h/.cpp` + shaders)

Instanced rendering: one draw call per frame. Each particle is a quad scaled/positioned in the vertex shader. The fragment shader renders circular particles with smooth edges using `fwidth + smoothstep`. Vsync is disabled (`glfwSwapInterval(0)`).

### Debug Renderer (`src/debug_renderer.h/.cpp`)

Lightweight immediate-mode line overlay. All primitives (lines, rects, crosses) are batched into a single VBO and drawn with `GL_LINES` in one call after the particle draw. Uses inline GLSL shaders (no separate shader files); coordinates are NDC, matching the rest of the app. Toggle with the `D` key or the **Debug vis** checkbox in the HUD.

What is drawn when enabled:
- **AABB colliders** — gray outline
- **Containers** — cyan outline; switches to green when any trigger bound to that container is active
- **Plane colliders** — short orange line segment perpendicular to the normal at the plane point
- **Lever arm** — yellow line from `leverEndB` to `leverEndA`
- **Lever pivot** — red crosshair

### Object Dragging (`src/main.cpp`)

Lightweight interactive placement of physics objects while the simulation is paused. Tracked with a small `DragState` struct and two single-frame flags (`g_left_just_pressed`, `g_left_just_released`) set in `mouse_button_callback` and consumed each frame.

Rules:
- Drag can only **start** while paused (`g_paused == true`). Pressing SPACE while dragging cancels the drag.
- `R` reset also cancels any active drag.
- **Containers** have higher selection priority than AABB colliders when the cursor overlaps both.
- Dragging preserves object size; only the center moves. Object bounds are updated in-place directly on the `InteractionSystem` vectors, so they take effect for the next simulation step.
- Dragged positions persist until the next reset (which does not restore positions — it only clears runtime state).

Mouse NDC coordinates are computed unconditionally once per frame and shared between drag logic and fluid force application.

The debug overlay automatically enables for the selected object regardless of `g_debug_vis` state.

### Builder (`src/main.cpp`)

Lightweight in-app object placement mode for placing Box Colliders and Containers while the simulation is paused. State is tracked with a small `BuildState` struct (`active`, `objectType`, `width`, `height`, `restitution`, `particleMass`, `capacity`).

Rules:
- Placement only works while paused. Unpausing auto-clears `build.active`.
- `R` reset also cancels placement mode.
- Left click places the object at the mouse position (center-anchored, size from `width`/`height`).
- Right click cancels placement mode.
- After placing, the mode stays active for repeated placement.
- Placed objects append to `interactionSystem.aabbColliders` or `interactionSystem.containers` directly — no secondary store.
- Placed objects persist across reset (consistent with existing reset semantics).
- Containers are named `"Container 1"`, `"Container 2"`, etc. via a `nextContainerIdx` counter.

The **Builder** ImGui window is always visible (top-right corner, `ImGuiCond_Once` initial position). It shows a status line (red = not paused, green = actively placing, gray = ready), object type radio buttons, size/parameter sliders, and a Cancel button.

The placement preview renders as a bright outline + center cross at the current mouse NDC position, visible whenever `build.active && g_paused` (independent of `g_debug_vis`).

### Application Layer (`src/main.cpp`)

Sets up GLFW window (640×480), compiles shaders, runs the render loop with a fixed-timestep accumulator. Handles:
- Keyboard: `SPACE` pause/resume, `RIGHT` single-step, `R` reset, `ESC` toggle HUD, `D` toggle debug visualization
- Mouse (running): left-click pull force, right-click push force on fluid
- Mouse (paused, no build mode): left-click + drag to move AABB colliders and containers
- Mouse (paused, build mode active): left-click places object at mouse; right-click cancels build mode; drag is suppressed
- ImGui HUD (ESC): particle count, physics sliders, debug vis toggle, interaction system state
- ImGui Builder (always visible): placement mode, object type, size/params, cancel

### Physics Parameters (`src/particle_config.h/.cpp`)

All tunable physics constants live here: `gravity`, `xsphC` (viscosity), `vorticityEpsilon`, `NUM_ITERATIONS`, `NUM_PARTICLES`, `smoothingRadius`. Kernel normalization constants (`h²`, `h⁵`, `poly6_norm`, `spiky_norm`) are precomputed here and consumed by `particles.cpp`.

### Math (`src/linear_algebra.h/.cpp`)

Custom `Vec2`, `Vec3`, `Mat4` types with all needed operations. No external math library.

## Current Limitations

- 2D only; no z-axis or 3D spatial hashing.
- One-way fluid–solid coupling: colliders correct particle positions as a post-solve pass, not inside the PBF constraint loop.
- No rigid body solver; levers and containers have no mass and do not react to fluid forces beyond the metrics they expose.
- The lever/trigger behaviour model is simplified and milestone-oriented — not a general constraint or event system.
- Debug visualization uses 1-pixel GL lines (GL Core profile does not guarantee wider lines on all drivers).

## Planned Extension

The interaction subsystem is designed to feed a future scene pipeline:
- Scene layout, camera, and 3D object placement will be handled by a separate scene layer.
- This subsystem exposes logical state (container `fillRatio`, trigger `active`, lever `angle`) as clean numeric values that a higher-level scene system can consume.
- The module boundaries (`collider`, `container`, `trigger`, `lever`, `interaction_system`) are intended to remain stable as the project evolves toward 3D; 3D variants would replace or extend these files without changing the aggregation pattern in `InteractionSystem`.

## Benchmarks

Google Benchmark v1.8.3 (fetched via CMakeFetch). Two benchmarks:
- `BM_ParticlesUpdate` — isolated simulation step, sweeps N=200→4000
- `BM_MainFrame_CPUOnly` — full frame at 60 fps fixed timestep

Results output as CSV to `benchmarks/data/` named by commit hash. Visualized via `benchmarks/benchmark_plotting.ipynb`.

## Dependencies

Bundled in `dependencies/`: GLAD (OpenGL loader), ImGui, GLFW headers, KHR headers. GLFW itself must be installed via system package manager or vcpkg. `src/glad.c` is the generated OpenGL loader source.
