# Fluid-Sim Project: Thesis-Quality Architecture Guide

> **Context:** This is a pure investigation and teaching pass — no code was modified. The goal is to give a first-time reader a complete mental model of the project so they can confidently implement new features like object placement, colliders, containers, triggers, and lever/Goldberg-machine interactions.

---

## 1. Project Overview

### What This Project Does

This is a **real-time 3D fluid simulation** built entirely from scratch in C++20 and OpenGL 3.3 Core Profile. It simulates fluids using the **Position-Based Fluids (PBF)** algorithm (Macklin & Müller 2013) — a technique used in games and VFX to make water, lava, and other fluids look realistic in real time.

The simulation:
- Maintains up to ~40,000 fluid particles at interactive frame rates
- Runs the PBF pressure/density solver every frame on the CPU using parallel threads
- Renders particles as shaded spherical billboards in 3D via a single instanced draw call
- Provides a 3D camera with orbit, pan, and zoom controls
- Has a physics interaction layer (colliders, containers, triggers, levers) — currently in newly-added untracked files (`src/physics/`)
- Includes a debug rendering overlay for physics object visualization

### Technologies Used

| Technology | Purpose | How Acquired |
|---|---|---|
| C++20 | Language; `std::execution::par_unseq` for parallel loops | Compiler |
| OpenGL 3.3 Core | GPU rendering | System |
| GLFW 3.4 | Window creation + input events | CMake FetchContent |
| GLAD | OpenGL function loader | Bundled in `dependencies/glad/` |
| Dear ImGui | Immediate-mode debug UI/HUD | Bundled in `dependencies/imgui/` |
| TBB (optional) | Parallel backend on macOS (replaces `par_unseq`) | find_package (optional) |
| Google Benchmark | Performance benchmarking | CMake FetchContent (when `BUILD_BENCHMARKS=ON`) |

### What the Main Executable Is

**`build/Release/fluid-sim.exe`** (~719 KB). Built from `src/main.cpp` linked against a shared library `fluid_core` (particles, linear algebra, config).

### What Happens When the Program Starts

```
1. GLFW initializes, creates a 640×480 OpenGL 3.3 Core window
2. ImGui is attached to the window
3. GLAD loads all OpenGL function pointers
4. Shaders compiled: vertex.glsl + fragment.glsl (particles) + grid shaders
5. Particles object created (N=40,000 particles, h=0.07 smoothing radius)
6. TriangleMesh created (instanced quad renderer)
7. Grid floor mesh created
8. Particle instance VBOs allocated on GPU (60,000 slot capacity)
9. reset() called → particles placed in 3D grid, spatial hash built
10. Main render loop begins (vsync OFF, uncapped FPS)
```

---

## 2. Folder and File Map

```
fluid-sim/
├── CMakeLists.txt          ← Build system (master config, 3 targets)
├── README.md               ← User-facing docs (controls, build steps, references)
├── CLAUDE.md               ← Architecture guide for Claude Code (newly added, untracked)
├── .gitignore              ← Excludes build/, CMakeFiles/, .DS_Store
├── .gitattributes          ← Marks glad.c + notebooks as linguist-vendored
├── imgui.ini               ← ImGui window layout (runtime-generated)
│
├── src/                    ← ALL application source
│   ├── main.cpp            ← ★ Entry point, window, main loop, input, shaders (516 lines)
│   ├── config.h            ← Common includes (imgui, glad, GLFW, std)
│   ├── helpers.h           ← randomFloat(), rgba_normalizer()
│   │
│   ├── particles.h         ← ★ Particles class declaration + PBF fields (89 lines)
│   ├── particles.cpp       ← ★ PBF solver: spatial hash, neighbors, constraint loop (538 lines)
│   ├── particle_config.h   ← Extern physics constants (gravity, smoothingRadius, etc.)
│   ├── particle_config.cpp ← Actual constant values
│   │
│   ├── linear_algebra.h    ← Vec2, Vec3, Mat4 type declarations
│   ├── linear_algebra.cpp  ← lookAt, perspective, cross, normalize, etc.
│   │
│   ├── triangle_mesh.h     ← TriangleMesh class: instanced particle renderer
│   ├── triangle_mesh.cpp   ← VAO/VBO setup, per-frame GPU upload, instanced draw call
│   │
│   ├── grid.h              ← Grid class: floor visualization mesh
│   ├── grid.cpp            ← Grid vertex/index generation
│   │
│   ├── cell.h              ← Cell struct (3D grid cell coords), CellEntry struct
│   ├── cell.cpp            ← Cell operator overloads
│   │
│   ├── colors.h            ← 4-stop color ramp (purple→cyan→yellow→red by velocity)
│   │
│   ├── debug_renderer.h    ← ★ DebugRenderer: immediate-mode line overlay (NEW, untracked)
│   ├── debug_renderer.cpp  ← ★ Line/rect/cross batching, inline GLSL, NDC draw (NEW)
│   │
│   ├── physics/            ← ★ Physics interaction subsystem (ALL NEW, untracked)
│   │   ├── collider.h/.cpp         ← PlaneCollider + AABBCollider + resolve functions
│   │   ├── container.h/.cpp        ← Container AABB region (count/mass/fillRatio)
│   │   ├── trigger.h/.cpp          ← Trigger: threshold on container metric
│   │   ├── lever.h/.cpp            ← Lever: 2D angular seesaw with dynamics
│   │   └── interaction_system.h/.cpp ← InteractionSystem: aggregates + drives update order
│   │
│   ├── glad.c              ← Generated OpenGL loader (compiled as C source)
│   │
│   └── shaders/
│       ├── vertex.glsl         ← Particle instanced vertex shader
│       ├── fragment.glsl       ← Particle sphere billboard fragment shader
│       ├── grid_vertex.glsl    ← Grid floor vertex shader
│       └── grid_fragment.glsl  ← Grid floor fragment shader (radial alpha fade)
│
├── dependencies/           ← ALL bundled third-party headers
│   ├── glad/               ← GLAD OpenGL loader headers
│   ├── GLFW/               ← GLFW3 API headers (glfw3.h, glfw3native.h)
│   ├── imgui/              ← Dear ImGui source + GLFW/OpenGL3 backends
│   └── KHR/                ← Khronos platform definitions
│
└── benchmarks/
    ├── benchmark.cpp       ← BM_ParticlesUpdate + BM_MainFrame_CPUOnly tests
    ├── run_benchmarks.ps1  ← Windows benchmark runner (cmake + run + CSV output)
    ├── run_benchmarks.sh   ← Unix benchmark runner
    ├── requirements.txt    ← Python: numpy, pandas, matplotlib
    ├── benchmark_plotting.ipynb ← Jupyter notebook: CSV → plots
    └── data/               ← 7 benchmark CSV result files (named by commit hash)
```

**Central files** (touch these most): `main.cpp`, `particles.cpp`, `particles.h`, `src/physics/interaction_system.cpp`

**Supporting files** (usually stable): `linear_algebra.*`, `triangle_mesh.*`, `particle_config.*`, shaders

---

## 3. Build System Explanation

### How CMakeLists.txt Works

The CMakeLists.txt defines **three CMake targets**:

```
fluid_core (LIBRARY) ─────┬─→ fluid-sim (EXECUTABLE)      [default build]
                           └─→ fluid-sim-bench (EXECUTABLE) [BUILD_BENCHMARKS=ON]
```

**`fluid_core`** is a static/object library containing everything shared:
- `particles.cpp`, `linear_algebra.cpp`, `particle_config.cpp`
- ImGui source files + GLFW/OpenGL3 backends
- Linked with TBB::tbb (if found, for macOS parallel support)

**`fluid-sim`** (the main app) adds:
- `src/main.cpp`, `src/glad.c`, `triangle_mesh.cpp`, `grid.cpp`
- Links against `fluid_core + glfw + OpenGL::GL`

**`fluid-sim-bench`** (benchmarks) adds:
- `benchmarks/benchmark.cpp`
- Links against `fluid_core + benchmark::benchmark`

### vcpkg Usage

vcpkg is **not integrated in CMakeLists.txt itself** — you pass it as a toolchain file when running cmake:

```bash
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

This tells CMake's `find_package(glfw3)` where to look. No `vcpkg.json` manifest file exists in the project.

### Dependency Acquisition

| Dependency | How Acquired |
|---|---|
| GLFW 3.4 | `FetchContent_Declare` from GitHub `glfw/glfw.git` tag `3.4` |
| Google Benchmark | `FetchContent_Declare` from `google/benchmark.git` tag `v1.8.3` |
| GLAD | Bundled in `dependencies/glad/` + `src/glad.c` compiled directly |
| ImGui | Bundled in `dependencies/imgui/` as source files |
| TBB | System package (`find_package(TBB QUIET)`) — optional |
| OpenGL | System (`find_package(OpenGL REQUIRED)`) |

### Exact Build/Run Commands on Windows

```bash
# Configure (Release mode, with vcpkg)
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --target fluid-sim

# Run
./build/Release/fluid-sim.exe

# Benchmarks
cmake -S . -B build-bench \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_BENCHMARKS=ON
cmake --build build-bench --target fluid-sim-bench
.\benchmarks\run_benchmarks.ps1
```

### Where the Final .exe Is Created

- Main app: `build/Release/fluid-sim.exe`
- Debug build: `build/Debug/fluid-sim.exe`
- Benchmarks: `build-bench/fluid-sim-bench.exe` (or `build-bench/Release/fluid-sim-bench.exe`)

---

## 4. Main Program Flow

### Initialization Sequence (`main.cpp`, lines ~139–229)

```
Step 1: GLFW setup
  glfwInit()
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3)
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3)
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE)
  glfwCreateWindow(640, 480, ...)
  glfwMakeContextCurrent(window)
  glfwSwapInterval(0)  ← vsync OFF (uncapped FPS)

Step 2: ImGui setup
  ImGui::CreateContext()
  ImGui_ImplGlfw_InitForOpenGL(window, true)
  ImGui_ImplOpenGL3_Init("#version 330")

Step 3: Register callbacks
  glfwSetKeyCallback(window, key_callback)
  glfwSetMouseButtonCallback(window, mouse_button_callback)
  glfwSetCursorPosCallback(window, cursor_position_callback)
  glfwSetScrollCallback(window, scroll_callback)
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback)

Step 4: OpenGL state
  gladLoadGLLoader(...)  ← load all GL function pointers
  glEnable(GL_DEPTH_TEST)
  glEnable(GL_BLEND)
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
  glClearColor(0,0,0,1)  ← black background

Step 5: Create simulation objects
  Particles particles(NUM_PARTICLES, smoothingRadius)
  TriangleMesh triangleMesh
  Grid grid(50 cells, 0.2 spacing, -1.0 z-offset)

Step 6: Compile shaders
  GLuint shaderProgram = make_shader("vertex.glsl", "fragment.glsl")
  GLuint gridShader    = make_shader("grid_vertex.glsl", "grid_fragment.glsl")

Step 7: Allocate GPU buffers
  triangleMesh.setupInstanceBuffers(60000)

Step 8: First reset
  reset(particles)  ← place particles in grid, build spatial hash

Step 9: Get start time for frame timing
  lastTime = glfwGetTime()
```

### Main Loop (each frame, lines ~236–438)

```
┌─── FRAME START ──────────────────────────────────────────────────────────┐
│                                                                           │
│  1. glfwPollEvents()                                                     │
│     → Fires keyboard/mouse callbacks → sets g_paused, g_reset, etc.     │
│                                                                           │
│  2. ImGui::NewFrame()                                                    │
│     Build HUD widgets (sliders for physics, FPS display)                │
│     Only visible when show_hud == true (ESC to toggle)                  │
│                                                                           │
│  3. glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)                  │
│                                                                           │
│  4. Frame timing                                                          │
│     dt = glfwGetTime() - lastTime   (clamped to 1/60 max)               │
│     If g_reset: reset(particles), g_paused=false                        │
│     If !g_paused: dt_to_sim = dt                                        │
│     If g_step_one: dt_to_sim = 1/60, g_step_one=false                  │
│                                                                           │
│  5. Camera matrix setup                                                   │
│     camX = g_panX + g_radius * cos(elevation) * sin(azimuth)           │
│     camY = g_panY + g_radius * sin(elevation)                           │
│     camZ = g_radius * cos(elevation) * cos(azimuth)                     │
│     viewMatrix = lookAt(cam, panPoint, up)                              │
│                                                                           │
│  6. Mouse ray (if g_push || g_pull)                                      │
│     x_ndc = (mouseX / winW) * 2 - 1                                     │
│     y_ndc = 1 - (mouseY / winH) * 2                                     │
│     Unproject ray through view/proj matrices to z=0 plane               │
│                                                                           │
│  7. Simulation step                                                       │
│     if (dt_to_sim > 0):                                                  │
│       particles.update(dt, smoothingRadius, radius_px, ...)             │
│     (interaction system update would go here for physics objects)        │
│                                                                           │
│  8. Upload particle data to GPU                                           │
│     Build positions[] and colors[] arrays                                │
│     triangleMesh.updateInstanceData(positions, colors)                   │
│                                                                           │
│  9. Render                                                                │
│     glUseProgram(shaderProgram)                                          │
│     Upload uniforms: projection, view, radius, lightDir                 │
│     triangleMesh.drawInstanced(nParticles)                              │
│     grid.draw(gridShader, projection, view)                             │
│                                                                           │
│  10. ImGui::Render() → ImGui_ImplOpenGL3_RenderDrawData()               │
│                                                                           │
│  11. glfwSwapBuffers(window)                                             │
│                                                                           │
└─── FRAME END ─────────────────────────────────────────────────────────────┘
```

### Keyboard Input Handling

`key_callback()` (lines ~38–57) — Only fires on `GLFW_PRESS`:

| Key | Effect | Global Changed |
|---|---|---|
| SPACE | Toggle pause/resume | `g_paused = !g_paused` |
| RIGHT ARROW | Advance one frame while paused | `g_step_one = true` |
| R | Request reset next frame | `g_reset = true` |
| ESC | Toggle ImGui HUD visibility | `show_hud = !show_hud` |

### Mouse Input Handling

`mouse_button_callback()` (lines ~59–95):
- ImGui gets first pick; if ImGui wants the mouse, returns immediately
- Left + CTRL → `g_panning = true`
- Left + SHIFT → `g_orbiting = true`
- Left (no mods) → `g_pull = true`
- Right → `g_push = true`

`cursor_position_callback()` (lines ~97–125):
- If orbiting: update azimuth and elevation (scaled by 0.005)
- If panning: update g_panX/g_panY by camera-relative delta

`scroll_callback()` (lines ~32–36):
- `g_radius -= yoffset * 0.2f` (clamped to ≥ 0.001)

### Cleanup (lines ~439–448)

```cpp
glDeleteProgram(shaderProgram)
glDeleteProgram(gridShader)
delete triangleMesh
ImGui_ImplOpenGL3_Shutdown()
ImGui_ImplGlfw_Shutdown()
ImGui::DestroyContext()
glfwDestroyWindow(window)
glfwTerminate()
```

---

## 5. Fluid Simulation Theory

### Position-Based Fluids in Simple Terms

Normal physics simulations compute forces → accelerate particles → move them. This works but can become unstable with large timesteps.

**PBF flips this around:**
1. Move particles freely (ignoring fluid constraints): "predicted position"
2. Measure how much each particle violates the incompressibility constraint (fluid shouldn't compress)
3. Compute corrective position offsets to push particles toward valid density
4. Iterate this correction a few times
5. Derive velocity from how far particles actually moved

This is inherently stable because you're solving constraints geometrically, not numerically integrating forces. The key insight: **fluids are incompressible** — every particle should have roughly the same density as its neighbors. If density is too high (particles squeezed), push them apart. If too low (particles spread), pull them together.

### The Smoothing Kernel Functions

SPH (Smoothed Particle Hydrodynamics) computes fluid properties by summing **kernel-weighted contributions** from neighbors within radius h:

```
       Σⱼ W(|xᵢ - xⱼ|, h) · (property of j)
value ≈ ─────────────────────────────────────
                   Σⱼ W(...)
```

The **kernel W** is a bell-shaped function that:
- Returns maximum weight when particles overlap (distance = 0)
- Returns zero weight at distance = h (the smoothing radius)
- Is smooth in between → produces smooth fluid behavior

Two kernels are used:
- **Poly6 kernel**: `W(r,h) = (315 / 64π h⁸) · (h² - r²)³`  — for density estimation
- **Spiky gradient kernel**: `∇W = (-45 / π h⁵) · (h - r)² · r̂`  — for pressure forces (gradient direction toward neighbor)

The **normalization constants** (`poly6_norm`, `spiky_norm`) are precomputed in `particle_config.cpp` so the inner loop is just multiplications.

### Simulation Step-by-Step

Every frame, `particles.update(dt, ...)` performs these steps in exact order:

#### Step 1: Velocity Integration (Symplectic Euler Prediction)

```
For each particle i:
  v_i += gravity * dt                          // gravity force
  v_i += mouseForce(i) * dt                   // optional mouse interaction
  p*_i = p_i + v_i * dt                       // predict position
```

`p*_i` is the "predicted position" — where the particle WOULD go if there were no pressure constraints. This is stored in `predictedPositions[]`.

#### Step 2: Rebuild Spatial Structures

```
buildGrid(h)                    // place each particle in its grid cell
if needsNeighbourRebuild():     // check skin radius threshold
  buildNeighbours(h)            // find all pairs within distance h
  save positionsAtLastBuild
```

This is an optimization: neighbor lists don't need to be rebuilt every frame (see Section 7).

#### Step 3: PBF Constraint Loop (NUM_ITERATIONS = 4 times)

```
Repeat 4 times:
  ┌─ Compute λ_i for all particles (parallel) ──────────────────────────┐
  │  density_i = Σⱼ W(|p*_i - p*_j|, h)      // sum kernel over neighbors
  │  C_i = density_i / ρ₀ - 1                  // density constraint violation
  │  λ_i = -C_i / (Σⱼ |∇W|² + ε)             // Lagrange multiplier
  └─────────────────────────────────────────────────────────────────────┘

  ┌─ Compute Δp_i for all particles (parallel) ─────────────────────────┐
  │  For each neighbor j:                                               │
  │    corr = scorr(p*_i, p*_j)               // tensile instability fix
  │    Δp_i += (λ_i + λ_j + corr) * ∇W       // pressure correction
  │  Δp_i /= ρ₀                               // normalize by rest density
  └─────────────────────────────────────────────────────────────────────┘

  ┌─ Apply corrections and clamp (parallel) ────────────────────────────┐
  │  p*_i += Δp_i                                                       │
  │  clamp p*_i to domain boundaries [-1,1]³                           │
  └─────────────────────────────────────────────────────────────────────┘
```

After 4 iterations, predicted positions are much closer to satisfying incompressibility.

#### Step 4: Velocity Update and Color

```
For each particle i:
  v_i = (p*_i - p_i) / dt     // velocity from position change
  colors[i] = getColor(v_i)   // speed → color ramp
  p_i = p*_i                  // commit new position
```

#### Step 5: XSPH Viscosity

XSPH smooths particle velocities by averaging with neighbors. This prevents particles from moving differently from their neighbors (reduces noise):

```
For each particle i:
  xsph = Σⱼ W(r_ij) * (v_j - v_i)     // weighted velocity difference
  xsph /= Σⱼ W(r_ij)                   // normalize
  v_i += xsphC * xsph                  // blend toward neighbor average
  (clamp |dv| to maxDv = 0.2)         // prevent instability
```

#### Step 6: Vorticity Confinement

Numerical dissipation in SPH kills small swirls (vortices). Vorticity confinement adds them back:

```
Compute ω_i = curl of velocity field at particle i    // vorticity
  ω_i = Σⱼ (v_j - v_i) × ∇W                          // curl via SPH

Apply vorticity confinement force:
  η = ∇|ω|                              // gradient of vorticity magnitude
  N = η / |η|                           // location vector
  f_vorticity = ε * (N × ω_i)           // Kelvin's vorticity equation
  v_i += f_vorticity * ε * dt
```

---

## 6. Data Structures

### The Particles Class (`src/particles.h`)

All simulation state lives in the `Particles` class. Here are its member variables and what they hold:

```
Per-particle arrays (all vectors<T> of size nParticles):

positions[]            Vec3  Current confirmed position
predictedPositions[]   Vec3  Predicted position during constraint solve
velocities[]           Vec3  Velocity vector
densities[]            float (declared but unused in current update path)
colors[]               Vec3  RGB color (mapped from speed)
allLambdas[]           float Lagrange multiplier λᵢ (recomputed each iteration)
deltas[]               Vec3  Position correction Δpᵢ (recomputed each iteration)
oldPositions[]         Vec3  Positions before current step
vorticity[]            Vec3  Computed curl ω_i
neighbourData[]        int   FLAT 2D array of neighbor indices:
                             row i starts at i * MAX_NEIGHBOURS (256)
                             neighbourData[i*256 + k] = index of k-th neighbor of i
neighbourCount[]       int   How many neighbors particle i has
positionsAtLastBuild[] Vec3  Positions at last neighbor rebuild (for skin radius check)

Spatial hash grid:
gridData[]             int   Flat sorted list of particle indices by cell
gridStart[]            int   Start index in gridData for each cell (prefix sum)
gridCount[]            int   Number of particles in each cell

Precomputed kernel constants:
h2, h5, h8             float h², h⁵, h⁸
poly6_norm             float 315 / (64π h⁸ h)
spiky_norm             float -45 / (π h⁵ h)
W_dq                   float poly6 kernel evaluated at r = 0.14h (for scorr)
skinRadius             float 0.3 * h (neighbor rebuild threshold)

Grid dimensions:
nCells1D               int   ceil(2.0 / h)  ← domain is [-1,1]³ = size 2
restDensity            float estimated density at the center (from estimateRestDensity())
```

### Why Flat Arrays?

The neighbor list could be stored as `vector<vector<int>>` (a 2D jagged array). Instead it's a flat `vector<int>` of size `nParticles × MAX_NEIGHBOURS`:

```
particle 0 neighbors: [neighborData[0], neighborData[1], ..., neighborData[255]]
particle 1 neighbors: [neighborData[256], neighborData[257], ..., neighborData[511]]
particle i neighbors: [neighborData[i*256 + 0], ..., neighborData[i*256 + 255]]
```

**Why?** CPU cache lines are 64 bytes. When the solver loops over particle i's neighbors, all 256 neighbor indices are in a contiguous memory region. The CPU can prefetch them efficiently. A vector-of-vectors scatters neighbor data across heap allocations → random cache misses → much slower.

### Key Config Constants (`particle_config.cpp`)

| Constant | Value | Meaning |
|---|---|---|
| `smoothingRadius` (h) | 0.07 | SPH kernel support radius in NDC units |
| `NUM_ITERATIONS` | 4 | PBF constraint iterations per frame |
| `NUM_PARTICLES` | 40000 | Default particle count |
| `gravity` | -3.5f | Downward acceleration (NDC/s²) |
| `xsphC` | 0.1f | XSPH viscosity coefficient |
| `vorticityEpsilon` | 1000.0f | Vorticity confinement strength |
| `k` | 0.000006f | Tensile instability correction coefficient |
| `RELAXATION_F` | 15000.0f | PBF denominator regularizer |
| `INIT_SPACING` | 0.03f | Grid spacing for initial particle placement |
| `radius_logical` | 13.0f | Particle visual size in pixels |
| `MAX_NEIGHBOURS` | 256 | Max neighbor list length per particle |

---

## 7. Neighbor Search / Spatial Hashing

### Why Neighbor Search Is Needed

The PBF solver and XSPH/vorticity both need to sum quantities over all nearby particles:

```
density_i = Σⱼ W(|p_i - p_j|, h)
```

With 40,000 particles, brute-force would check all pairs: **40,000 × 40,000 = 1.6 billion distance calculations per solver iteration**. That's impossibly slow.

The SPH kernel W goes to zero at distance h. So for particle i, only particles within distance h contribute. If we can find those O(k) neighbors quickly — where k is the average neighbor count (~20–50 in a typical fluid) — the sum is O(kN) instead of O(N²).

### How the Grid Works

The simulation domain is the cube [-1,1]³ (size 2 in each axis). The grid divides this into cells of size h:

```
nCells1D = ceil(2.0 / h) = ceil(2.0 / 0.07) = 29

Total cells = 29³ = 24,389
```

Each particle is assigned to exactly one cell based on its position:

```cpp
// positionToCoord() in particles.cpp
cx = (position.x + 1) / h    // shift from [-1,1] to [0, 2/h]
cy = (position.y + 1) / h
cz = (position.z + 1) / h
// clamp to [0, nCells1D - 1]
```

### Building the Grid (Two-Pass Construction)

**Pass 1 — Count particles per cell:**
```
gridCount[cellOf(particle_0)]++
gridCount[cellOf(particle_1)]++
...
```

**Pass 2 — Compute prefix sums (start indices):**
```
gridStart[0] = 0
gridStart[1] = gridStart[0] + gridCount[0]
gridStart[2] = gridStart[1] + gridCount[1]
...
```

**Pass 3 — Fill sorted array:**
```
gridData[gridStart[cellOf(i)]++] = i    // insert particle i
```

After this, `gridData[gridStart[c] .. gridStart[c+1]-1]` contains all particle indices in cell c. Total time: O(N).

### Finding Neighbors (27-Cell Stencil Query)

For particle i in cell (cx, cy, cz):

```
For dx in {-1, 0, 1}:
  For dy in {-1, 0, 1}:
    For dz in {-1, 0, 1}:    // 27 cells total
      neighbor_cell = (cx+dx, cy+dy, cz+dz)
      For each particle j in neighbor_cell:
        if |p_i - p_j|² < h²:
          neighbourData[i * MAX_NEIGHBOURS + neighbourCount[i]++] = j
```

Since cells are size h, any particle within distance h must be in one of the 27 surrounding cells. No particle farther than h√3 (~1.73h) can be a neighbor.

### The Skin Radius Optimization

Rebuilding neighbors is expensive (O(N × 27k)). But particles don't move much between frames.

`needsNeighbourRebuild()` checks: has any particle moved more than `skinRadius = 0.3h` since the last rebuild? If not, the old neighbor list is still valid — particles that were close before are still close enough.

Result: neighbor rebuild cost drops from ~22ms to ~0.2–1.7ms for most frames.

---

## 8. Rendering Pipeline

### Overall Render Order Per Frame

```
1. Clear color + depth buffers
2. Upload instance data (positions[] + colors[] → GPU VBOs)
3. Draw instanced particles (1 draw call)
4. Draw grid floor
5. Draw debug overlays (DebugRenderer)
6. Render ImGui HUD
7. Swap buffers
```

### Particle Rendering: Instanced Quads

**The CPU side:**
```cpp
// TriangleMesh: one quad template stored in GPU
// Vertices: (-1,-1), (1,-1), (-1,1), (1,1)  — 2x2 unit quad
// Indices: [0,1,2, 2,1,3]  — two triangles (6 indices)

// Each frame:
glBufferSubData(instancePosVBO,   positions[], nParticles)
glBufferSubData(instanceColorVBO, colors[],    nParticles)
glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, nParticles)
```

One call draws N quads. The GPU runs the vertex shader N×4 times (4 vertices per quad × N instances), with `gl_InstanceID` selecting which particle's position/color to use.

**Vertex Shader (`vertex.glsl`):**

```glsl
layout(location=1) in vec3 instancePos;    // per-instance: particle world position
layout(location=2) in vec3 instanceColor;  // per-instance: particle color
layout(location=0) in vec3 vertexPos;      // per-vertex: quad corner (-1..1)

void main() {
  vec4 viewPos = view * vec4(instancePos, 1.0);  // particle to view space
  viewPos.xy += vertexPos.xy * radius;            // expand quad by radius
  gl_Position = projection * viewPos;            // project to clip space
  uv = vertexPos.xy;                             // pass to fragment
}
```

Key insight: **the quad is expanded in view space**, not world space. This means particles always face the camera (they are billboards). The radius is added to the already-view-space position, so perspective doesn't shrink it differently in x vs y.

**Fragment Shader (`fragment.glsl`):**

```glsl
void main() {
  vec2 n_xy = uv * 2.0;                         // [-2,2] space
  float r = length(n_xy);
  float w = fwidth(r);                           // anti-alias edge width
  float a = 1.0 - smoothstep(1.0-w, 1.0+w, r); // soft circle mask

  float z = sqrt(max(0.0, 1.0 - r*r));          // sphere height
  vec3 normal = normalize(vec3(n_xy, z));        // fake sphere normal
  float diffuse = max(dot(normal, lightDir), 0);
  vec3 color = vColor * (0.7 + 0.7*diffuse);    // ambient + diffuse
  
  if (a < 0.5) discard;                         // clip non-circle pixels
  screenColor = vec4(color, a);
}
```

Each particle is rendered as a **sphere billboard**: the fragment shader simulates a 3D sphere by treating the 2D quad distance from center as a sphere radius, computing a fake z height, and shading with a directional light. The `fwidth + smoothstep` provides pixel-perfect anti-aliasing at the circle edge.

### Grid Floor Rendering

The grid (`src/grid.h/.cpp`) is a flat mesh of quads at z = -1.0 (below the simulation domain). The fragment shader fades it with distance using `smoothstep(3.0, 7.0, dist)`, creating a radial alpha fade that lets the background show through at the edges.

### Debug Renderer (`src/debug_renderer.h/.cpp`)

An **immediate-mode line renderer**. Every frame you call `addLine()`, `addRect()`, `addCross()` to queue primitives, then `flush()` to batch-upload them and draw with a single `glDrawArrays(GL_LINES, ...)`.

Key design: it operates in **NDC coordinates** (x,y ∈ [-1,1]). The inline vertex shader passes positions directly to `gl_Position` with no transformation:

```glsl
gl_Position = vec4(aPos, 0.0, 1.0);  // NDC passthrough
```

This means debug shapes align with the simulation domain (which also uses [-1,1] as the simulation boundary) — no coordinate conversion needed.

Interleaved vertex format: `[x, y, r, g, b, a]` — 6 floats per vertex, 24 bytes.

### Coordinate System Summary

```
World Space:   [-1, 1]³ cube   ← simulation domain AND debug renderer NDC
View Space:    camera-relative, z points into screen
Clip Space:    NDC after projection (what OpenGL sees)
Screen Space:  pixel coordinates (0..640, 0..480)

Particle positions are stored in [-1,1]³ (world/NDC directly)
Mouse position converted to NDC: x = (mouseX / 640) * 2 - 1
```

---

## 9. Input and Interaction System

### Complete Control Reference

| Input | State | Effect |
|---|---|---|
| SPACE | Any | Toggle pause/resume |
| RIGHT ARROW | Paused | Single-step one frame |
| R | Any | Set g_reset flag → reset next frame |
| ESC | Any | Toggle ImGui HUD visibility |
| Left mouse | Running | `g_pull = true` → pull particles toward cursor |
| Right mouse | Running | `g_push = true` → push particles away from cursor |
| Left + SHIFT | Any | `g_orbiting = true` → orbit camera with cursor |
| Left + CTRL | Any | `g_panning = true` → pan camera with cursor |
| Scroll wheel | Any | Zoom in/out (g_radius ± 0.2/scroll) |

### Mouse Force Implementation

When `g_pull` or `g_push` is active, `main.cpp` computes a 3D ray from the camera through the cursor position and finds where it intersects the z=0 plane. The world position of this intersection is passed to `particles.update()` as `mouseRayOrigin` and `mouseRayDir`.

Inside `particles.update()`, for each particle the perpendicular distance to the ray is computed. If distance < mouseRadius, a velocity impulse is added proportional to `(1 - dist/radius)` (linear falloff).

### Where to Hook New Features

New object placement/selection logic should be added to:
1. `main.cpp` — add new keyboard/mouse handlers
2. `src/physics/interaction_system.cpp` — add update calls for new object types
3. The interaction system vectors (`aabbColliders`, `containers`, etc.) are already exposed as `public` vectors — you can push new objects in at any time

The object drag system (DragState) and builder system (BuildState) described in CLAUDE.md are planned for main.cpp and follow this pattern:
- State tracked in a small struct
- Mouse callbacks check struct state to determine what action to take
- Dragged/placed objects modify the InteractionSystem vectors directly

### Why Objects Might Be Invisible / Hard to Interact With

Key reasons objects can appear invisible:

1. **Wrong coordinate space**: If a collider is placed at pixel coordinates (e.g., x=320) instead of NDC (x=0.0), it's far outside the simulation domain.
2. **DebugRenderer not flushed**: `debugRenderer.flush()` must be called each frame after all `addLine()` calls but before `glfwSwapBuffers()`.
3. **Debug vis disabled**: The debug overlay is only drawn when `g_debug_vis == true`.
4. **Z-fighting with particles**: Since debug renderer uses z=0, and particles also render near z=0, depth buffer order matters. Draw debug overlay AFTER particles.
5. **Alpha blending order**: Debug lines are opaque — render them last to avoid being blended away.

---

## 10. Existing Collision / Boundary Logic

### Built-in Boundary Clamping (`particles.cpp: clampToBoundaries`)

The fundamental boundary is the box [-1, 1]³. After each position update in the constraint loop, `clampToBoundaries()` is called:

```cpp
halfX = radius_px / g_fb_w   // particle radius in NDC units
pos->x = clamp(pos->x, -1 + halfX, 1 - halfX)
pos->y = clamp(pos->y, -1 + halfY, 1 - halfY)
pos->z = clamp(pos->z, -1 + halfZ, 1 - halfZ)
```

This is a **hard clamp** — particles teleport back to the boundary. No bounce velocity response is applied here. The velocity is implicitly corrected because PBF derives velocity from `(new_pos - old_pos) / dt` — if the particle was clamped, its velocity is automatically reduced.

### Physics Interaction Colliders (`src/physics/collider.h/.cpp`)

These are called as a **post-solve pass** from `InteractionSystem::update()` AFTER the PBF solver has run. They provide more realistic collision response:

**PlaneCollider** (half-space):
```
struct PlaneCollider { Vec2 point; Vec2 normal; float restitution; }

resolve:
  signedDist = dot(pos - point, normal)
  if signedDist < 0:              // penetrating the half-space
    pos += normal * (-signedDist) // push out
    vn = dot(vel, normal)         // velocity along normal
    vel -= normal * vn * (1 + restitution)  // reflect with damping
```

**AABBCollider** (solid box):
```
struct AABBCollider { Vec2 min; Vec2 max; float restitution; }

resolve:
  if point inside box:
    find face with minimum penetration depth (left, right, top, bottom)
    push point to nearest face
    zero (and reflect if restitution > 0) the velocity component toward that face
```

Both colliders work in 2D (Vec2). This matters for the 3D simulation — colliders only constrain x,y, not z. Extending them to 3D would require changing to Vec3.

### The Post-Solve Architecture

The separation between PBF solve and collision resolution is intentional:

```
Frame N:
  PBF solver (4 iterations of density constraints)  → predictedPositions
  Update velocities from positions
  Post-solve: InteractionSystem.update()
    → Apply PlaneCollider and AABBCollider to ALREADY-SOLVED positions
    → Update containers
    → Evaluate triggers
    → Update levers
```

This is **one-way coupling**: colliders push particles, but particles don't push colliders. It's an approximation — a rigid body sim would require bidirectional coupling — but it's cheap and looks good for fluid.

### Limitations

1. **Post-solve colliders can be overridden by the PBF solver**: If a particle is pushed out by a collider but the PBF density constraint wants it back inside, the next frame's PBF step might move it back. Fix: run colliders INSIDE the PBF loop (expensive).
2. **2D colliders in a 3D simulation**: PlaneCollider and AABBCollider use Vec2. Particles have a z coordinate that colliders ignore.
3. **No friction**: Collider response is pure velocity reflection with restitution.
4. **No particle-to-particle collision**: SPH pressure prevents overlap but doesn't guarantee rigid contact.

---

## 11. Benchmarks and Performance

### What Is Measured

Two Google Benchmark tests (`benchmarks/benchmark.cpp`):

**`BM_ParticlesUpdate`**: Isolates the simulation step.
- Creates a Particles object, runs 60 warm-up frames, then times `particles.update(...)` per iteration
- Sweeps N = 200, 500, 1000, 2000, 4000 particles
- Measures wall-clock time in milliseconds per simulation step

**`BM_MainFrame_CPUOnly`**: Simulates a full 60 FPS game loop (no rendering).
- Uses a fixed-timestep accumulator (FIXED_DT = 1/280, frame dt = 1/60)
- 120-frame warm-up, then times per iteration
- Intentionally reads output to prevent optimizer elimination

### How to Run Benchmarks on Windows

```powershell
# Option 1: PowerShell script (recommended)
.\benchmarks\run_benchmarks.ps1
# → builds automatically, outputs benchmarks/data/bench_<commithash>.csv

# Option 2: Manual
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON
cmake --build build-bench --target fluid-sim-bench
.\build-bench\Release\fluid-sim-bench.exe --benchmark_repetitions=5
```

### What Results Mean

From the 7 existing benchmark CSVs, example results:

```
BM_ParticlesUpdate/200    → ~0.13ms per update (200 particles)
BM_ParticlesUpdate/4000   → ~2.6ms per update (4000 particles)
```

The slope of `time vs N` should be **linear (O(N))** for the spatial hash approach. If you add an O(N²) step, the plot will curve upward. Use this as a regression detector.

The benchmark script outputs CSVs named `bench_<commithash>.csv`. The Jupyter notebook `benchmark_plotting.ipynb` reads these and plots mean ± stddev error bars.

---

## 12. Feature-Implementation Roadmap

Here's a clean plan for adding all Goldberg-machine features:

### Phase 1: Object Placement Menu

**Goal**: A UI panel showing buttons for "Box Collider", "Container", "Trigger", "Lever". Clicking one enters placement mode. Left-click in the simulation places the object. The existing BuildState pattern in CLAUDE.md is the right approach.

**Steps**:
1. Add `BuildState` struct to `main.cpp` (fields: `active`, `objectType`, `width`, `height`, `restitution`, `particleMass`, `capacity`)
2. Add `BuilderWindow` ImGui panel (always visible, top-right)
3. Add left-click placement handler in mouse callback (only active when paused + build.active)
4. Add placement preview in DebugRenderer (outline at mouse position)
5. Push new objects into `interactionSystem.aabbColliders` or `interactionSystem.containers`

### Phase 2: Draggable Objects

**Goal**: When paused, left-click an existing object to drag it.

**Steps**:
1. Add `DragState` struct (fields: `active`, `objectType`, `objectIdx`, `lastMouseNDC`)
2. In mouse callback (paused, no build mode): hit-test containers first, then AABB colliders
3. In cursor callback: if dragging, update object center by delta
4. In mouse release: stop drag

### Phase 3: Triggers and Levers

**Goal**: GUI to connect a trigger to a container, and a lever to a trigger.

**Steps**:
1. Add trigger creation to BuildState (specify container index + threshold)
2. Add lever creation (specify pivot, armLength, source type)
3. InteractionSystem already handles the update; just need objects pushed into it

### Phase 4: Debug Visualization Polish

**Goal**: Visual feedback for all physics objects.

**Steps** (DebugRenderer already exists):
1. Each frame after `interactionSystem.update()`, call:
   - `debugRenderer.addRect(collider.min, collider.max, gray)` for AABB colliders
   - `debugRenderer.addRect(container.min, container.max, triggered ? green : cyan)` for containers
   - `debugRenderer.addLine(leverEndA, leverEndB, yellow)` for levers
   - `debugRenderer.addCross(lever.pivot, 0.02, red)` for lever pivots
2. Call `debugRenderer.flush()` after particles are drawn

### Phase 5: Save/Load Scene

**Goal**: Serialize physics objects to JSON/text file, load on startup.

**Approach**:
1. Write a `serializeScene(interactionSystem) → string` function
2. Write a `deserializeScene(string) → InteractionSystem` function
3. Format: simple JSON (nlohmann/json or hand-written) with arrays of each object type
4. Add "Save Scene" and "Load Scene" buttons to the HUD

---

## 13. Best Files to Modify Later

| Feature | File(s) to Modify | Why | Risk Level | Notes |
|---|---|---|---|---|
| Builder UI panel | `src/main.cpp` | ImGui HUD lives here | Low | Add BuildState struct + ImGui window |
| Place AABB collider | `src/main.cpp`, `src/physics/interaction_system.h` | Placement logic in main, push to IS vectors | Low | IS.aabbColliders is public vector |
| Place container | `src/main.cpp` | Same pattern as AABB collider | Low | Auto-name with counter |
| Drag objects | `src/main.cpp` | DragState struct + mouse callbacks | Medium | Container gets priority over collider in overlap |
| Debug draw colliders | `src/main.cpp` + `src/debug_renderer.cpp` | Loop over IS.aabbColliders, call addRect | Low | NDC coordinates must match simulation space |
| Trigger creation | `src/main.cpp`, `src/physics/trigger.h` | Push Trigger into IS.triggers | Low | Need to specify containerIdx + threshold |
| Lever dynamics | `src/physics/lever.cpp` | Change physics parameters | Low | Stiffness/damping/maxAngle tuning |
| Lever rendering | `src/main.cpp` | Draw line from leverEndA to leverEndB | Low | Use DebugRenderer.addLine |
| 3D colliders | `src/physics/collider.h/.cpp` | Extend Vec2 → Vec3 | High | Breaks existing plane/AABB resolve logic |
| Inside-PBF colliders | `src/particles.cpp: update()` | Call collider resolve inside constraint loop | High | Could destabilize PBF solver |
| Scene serialization | New file `src/scene.h/.cpp` | Keep main.cpp clean | Medium | Prefer simple format first |
| Particle count resize | `src/particles.cpp: resizeParticles()` | Exists but needs grid/neighbor rebuild | Medium | Call while paused only |

---

## 14. Architecture Suggestions for Goldberg Machine System

### Recommended Object Type Enum

```cpp
// In a new src/scene.h:
enum class SceneObjectType {
    AABBCollider,
    PlaneCollider,
    Container,
    Trigger,
    Lever,
};
```

### Scene Configuration

Rather than extending `main.cpp`, consider a `SceneConfig` that holds all object definitions and is loaded at startup:

```cpp
struct SceneConfig {
    std::vector<AABBCollider>  aabbColliders;
    std::vector<PlaneCollider> planeColliders;
    std::vector<Container>     containers;
    std::vector<Trigger>       triggers;
    std::vector<Lever>         levers;
    std::vector<LeverSource>   leverSources;

    static SceneConfig loadFromFile(const std::string& path);
    void saveToFile(const std::string& path) const;
};
```

Then `InteractionSystem` is initialized from `SceneConfig`. This cleanly separates configuration (what objects exist) from runtime state (current counts, angles, active flags).

### UI/Menu System

The existing ImGui approach is correct and should be extended:

```
┌── ImGui Windows ─────────────────────────────────────────────────────────┐
│                                                                           │
│  Left side:  Main HUD (ESC toggle)                                       │
│    - FPS, particle count                                                  │
│    - Physics sliders (gravity, viscosity, etc.)                          │
│    - Interaction system debug panel (already exists in IS::renderDebugUI)│
│                                                                           │
│  Right side: Builder (always visible)                                    │
│    - Status line (red/green/gray)                                        │
│    - Object type radio buttons                                           │
│    - Size/parameter sliders                                              │
│    - Place / Cancel buttons                                              │
│                                                                           │
│  Popup (on right-click selected object): Properties editor              │
│    - Resize, restitution, threshold, etc.                                │
└───────────────────────────────────────────────────────────────────────────┘
```

### Serialization Approach

For a simple first pass, use a plain text format that can be hand-edited:

```
# scene.txt
AABB  -0.5 -0.8  0.5 -0.7  0.3   # min_x min_y max_x max_y restitution
CONTAINER  -0.3 -0.9  0.3 -0.7  1.0  100  # min max mass capacity
TRIGGER  fill_trigger  FillRatio  0.5  Persistent  0
LEVER  0.0 0.0  0.4  1.5  100.0  2.0  0.5  fill_trigger
```

Later, migrate to JSON using a small header-only library.

---

## 15. Questions / Unknowns

### Open Questions

1. **Are the physics/ and debug_renderer files integrated into main.cpp yet?**
   - `src/physics/` and `src/debug_renderer.*` are untracked (not committed).
   - CLAUDE.md describes full integration (DragState, BuildState, InteractionSystem::update in the loop, DebugRenderer::flush after particle draw).
   - The explore agents' main.cpp analysis showed the 3D camera but didn't confirm InteractionSystem calls in the main loop.
   - **Action**: Read main.cpp directly to confirm which calls are present before implementing anything.

2. **Is the physics interaction system 2D-only (Vec2) or 3D?**
   - PlaneCollider and AABBCollider use Vec2. Container.min/max are Vec2.
   - The simulation is 3D (particles have x,y,z). Colliders only constrain xy.
   - **Implication**: A new box collider placed at z≠0 won't work without Vec3 extension.

3. **What is `g_debug_vis`?**
   - CLAUDE.md mentions `D` key toggles it. The key callback analysis only showed SPACE, RIGHT, R, ESC.
   - Either the `D` key is in main.cpp but was missed in the analysis, or it's planned.
   - **Action**: Search main.cpp for `g_debug_vis` or `GLFW_KEY_D`.

4. **What does the `colors.h` gradient look like?**
   - 4-stop gradient: purple → cyan → yellow → red
   - Mapped from velocity magnitude 0 → MAX_SPEED (3.0)
   - **Implication**: Very slow particles appear purple/blue; fast particles appear red/orange.

5. **Is `CMakeLists.txt` already listing the physics/ source files?**
   - The agent reported `fluid_core` sources as: particles.cpp, linear_algebra.cpp, particle_config.cpp
   - The physics/*.cpp files are new and may not be in CMakeLists.txt yet
   - **Action**: Check CMakeLists.txt for `physics/` entries before building.

### Assumptions Made

- The simulation domain is treated as [-1,1]³ in NDC/world space
- Particle radius is stored in pixels (`radius_logical = 13.0f`) and converted to NDC per frame
- The interaction system uses 2D coordinates (Vec2) even in the 3D simulation
- `restDensity` is estimated empirically by querying the center particle, not set analytically

### Areas Needing Careful Testing Before New Features

1. **Adding objects while simulation is running**: The InteractionSystem vectors are not thread-safe. Only add objects while paused.
2. **AABB collider min/max in NDC**: Easy to accidentally use pixel or world units. Always verify with a known test shape (e.g., min={-0.5,-0.5}, max={0.5,0.5} should form a visible box in the center).
3. **Trigger threshold calibration**: `fillRatio = count / capacity`. If capacity is wrong, triggers never fire or always fire.
4. **Lever dynamics stability**: Very high `stiffness` or `loadThreshold = 0` can cause angular velocity explosion. Start with small values.
5. **DebugRenderer coordinate alignment**: DebugRenderer uses NDC passthrough. Container/collider min/max must be in the same NDC coordinate system.

---

## Next Implementation Plan: Object Placement Menu (First Feature)

### Goal

Add a "Builder" ImGui panel (always visible, top-right corner) that lets the user:
1. Select an object type (Box Collider or Container)
2. Adjust size/parameter sliders
3. Pause the simulation
4. Left-click to place the object at the cursor position
5. Right-click to cancel placement

### Prerequisite Check

Before coding, verify:
- [ ] Is `src/physics/interaction_system.h` included in `main.cpp`?
- [ ] Is `interactionSystem` instantiated as a global or local in main()?
- [ ] Is `interactionSystem.update()` called in the main loop?
- [ ] Are `src/physics/*.cpp` listed in `CMakeLists.txt`?
- [ ] Is `src/debug_renderer.cpp` listed in `CMakeLists.txt`?

### Files to Modify

1. **`CMakeLists.txt`** (if physics files not yet listed): add all `src/physics/*.cpp` and `src/debug_renderer.cpp` to `fluid-sim` target sources
2. **`src/main.cpp`**: Add BuildState struct, Builder ImGui window, mouse placement handler, placement preview via DebugRenderer
3. **No changes needed** to `src/physics/*.cpp` — they already support push_back to their vectors

### Implementation Steps

```
Step 1: Add to main.cpp (near globals):
  struct BuildState {
    bool active = false;
    SceneObjectType objectType = SceneObjectType::AABBCollider;
    float width = 0.4f;
    float height = 0.1f;
    float restitution = 0.3f;
    float particleMass = 1.0f;
    int   capacity = 100;
  } build;

Step 2: Add "Builder" ImGui window (inside the show_hud block OR always visible):
  ImGui::Begin("Builder", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  // status line: red if not paused, green if active, gray if ready
  // radio buttons: AABBCollider | Container
  // sliders: Width, Height, Restitution (or Capacity for Container)
  // Button: "Start Placing" / "Cancel"
  ImGui::End();

Step 3: In mouse_button_callback, add:
  if (build.active && g_paused && button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS):
    place object at mouseNDC with build.width/height
    push to interactionSystem.aabbColliders or containers
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS):
    build.active = false

Step 4: In the render loop, after particle draw:
  if (build.active && g_paused):
    debugRenderer.addRect(mouseNDC - half_size, mouseNDC + half_size, 1,1,1,1) // preview
  debugRenderer.flush()  // must be called every frame

Step 5: In R key handler:
  build.active = false  // cancel placement on reset
  
Step 6: In SPACE handler (when unpausing):
  build.active = false  // cancel placement when unpausing
```

### Testing the Feature

1. Build the project
2. Launch `fluid-sim.exe`
3. Press ESC to open HUD, confirm Builder panel is visible
4. Click "Start Placing" with Box Collider selected
5. Press SPACE to pause (or Builder should show "not paused" warning)
6. Move mouse — should see white outline preview following cursor
7. Left-click — object should appear and stay (debug vis must show it)
8. Resume simulation — particles should bounce off the new collider
9. Press R — placed objects persist (reset only clears runtime state, not configured objects)
