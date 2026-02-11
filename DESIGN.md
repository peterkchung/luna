# Luna — Design Document

Technical reference for Luna's architecture, module design, rendering pipeline, and data sources.

---

## Project Structure

```
luna/
├── CMakeLists.txt
├── LICENSE
├── README.md
├── DESIGN.md
├── docs/
│   ├── building.md
│   └── data-sources.md
│
├── assets/
│   ├── terrain/                # NASA LOLA heightmaps (downloaded at build time)
│   │   └── README.md           # Instructions for obtaining data
│   ├── ephemeris/              # SPICE kernels (downloaded at build time)
│   │   └── README.md
│   └── models/                 # OBJ meshes (lander, cockpit frame)
│       ├── cockpit.obj
│       └── lander.obj
│
├── shaders/
│   ├── terrain.vert
│   ├── terrain.frag
│   ├── skybox.vert
│   ├── skybox.frag
│   ├── celestial.vert          # Point sprites for stars, planets
│   ├── celestial.frag
│   ├── cockpit.vert
│   ├── cockpit.frag
│   ├── particle.vert
│   ├── particle.frag
│   ├── hud.vert
│   ├── hud.frag
│   └── trajectory.vert/.frag   # Predicted flight path overlay
│
├── src/
│   ├── main.cpp                # Entry point, app lifecycle
│   │
│   ├── core/                   # Vulkan infrastructure
│   │   ├── VulkanContext.h/cpp      # Instance, device, queues
│   │   ├── Swapchain.h/cpp          # Swapchain + recreation
│   │   ├── RenderPass.h/cpp         # Render pass + depth attachment
│   │   ├── Pipeline.h/cpp           # Pipeline builder (configurable)
│   │   ├── Buffer.h/cpp             # Buffer creation, upload, staging
│   │   ├── CommandPool.h/cpp        # Command buffer management
│   │   ├── DescriptorManager.h/cpp  # Descriptor sets, pools, layouts
│   │   ├── ShaderModule.h/cpp       # SPIR-V loading
│   │   ├── Image.h/cpp              # Image/texture creation + views
│   │   └── Sync.h/cpp               # Fences, semaphores, frame sync
│   │
│   ├── scene/                  # Scene objects and management
│   │   ├── Scene.h/cpp              # Scene graph, draw ordering
│   │   ├── Mesh.h/cpp               # Vertex/index buffer pair + draw
│   │   ├── MeshLoader.h/cpp         # OBJ loading (tinyobjloader)
│   │   ├── Terrain.h/cpp            # Heightmap mesh generation + LOD
│   │   ├── CelestialSphere.h/cpp    # Stars, planets, sun, earth
│   │   ├── ParticleSystem.h/cpp     # Exhaust particles
│   │   └── Cockpit.h/cpp            # Static cockpit frame geometry
│   │
│   ├── sim/                    # Simulation (no Vulkan dependencies)
│   │   ├── SimState.h/cpp           # Central simulation state
│   │   ├── Physics.h/cpp            # 6DOF rigid body, gravity, thrust
│   │   ├── OrbitalMechanics.h/cpp   # Keplerian orbits, transfer calcs
│   │   ├── Guidance.h/cpp           # Autopilot / trajectory prediction
│   │   ├── Ephemeris.h/cpp          # SPICE wrapper for body positions
│   │   └── TimeManager.h/cpp        # Mission elapsed time, UTC, dt
│   │
│   ├── input/                  # Input handling
│   │   ├── InputManager.h/cpp       # GLFW key/mouse polling
│   │   └── FlightControls.h/cpp     # Maps inputs to sim commands
│   │
│   ├── camera/                 # Camera system
│   │   ├── Camera.h/cpp             # Perspective projection + view matrix
│   │   └── CameraController.h/cpp   # Modes: cockpit, chase, free-look
│   │
│   ├── hud/                    # Heads-up display
│   │   ├── HudRenderer.h/cpp        # Screen-space quad pipeline
│   │   ├── Instruments.h/cpp        # Altitude, velocity, attitude, fuel
│   │   ├── AttitudeIndicator.h/cpp  # Artificial horizon (dynamic mesh)
│   │   ├── TrajectoryDisplay.h/cpp  # Predicted path overlay
│   │   └── NavBall.h/cpp            # Orientation reference sphere
│   │
│   └── util/                   # Shared utilities
│       ├── Math.h                   # GLM extensions, quaternion helpers
│       ├── FileIO.h/cpp             # File reading, path resolution
│       ├── Config.h/cpp             # Runtime configuration (JSON/TOML)
│       └── Log.h/cpp                # Lightweight logging
│
├── external/                   # Third-party (git submodules or fetched)
│   ├── glm/
│   ├── glfw/
│   ├── tinyobjloader/
│   ├── stb/                    # stb_image for heightmap TIFF loading
│   ├── cspice/                 # NASA SPICE toolkit (C)
│   └── toml++/                 # Config parsing
│
└── tools/
    ├── fetch_terrain.py        # Downloads NASA LOLA data + crops regions
    ├── fetch_kernels.py        # Downloads required SPICE kernel files
    └── heightmap_to_mesh.py    # Offline preprocessing for terrain LOD
```

---

## Module Dependencies

```
luna_sim    → luna_util, cspice         (no Vulkan)
luna_core   → luna_util, Vulkan, GLFW
luna_scene  → luna_core, luna_util
luna_camera → luna_core, luna_util
luna_hud    → luna_core, luna_util
luna_input  → luna_util, GLFW
main        → everything
```

The key property: `luna_sim` has no Vulkan dependency. This means:
- Physics can be unit tested without a GPU
- Contributors can work on simulation logic on any machine
- The sim module could be reused in a headless training environment

---

## Module Responsibilities

### core/ — Vulkan Infrastructure

Thin wrappers around Vulkan objects. Each class owns its resources and cleans up in its destructor (RAII). No rendering logic — just resource management.

**VulkanContext** owns the instance, physical device, logical device, and queues. Everything else receives a reference to it. Think of it like the DI container for Vulkan — one object that answers "what GPU are we using and how do I talk to it?"

**Pipeline** uses a builder pattern:
```cpp
auto pipeline = Pipeline::Builder(context, renderPass)
    .setShaders("terrain.vert.spv", "terrain.frag.spv")
    .setVertexFormat<TerrainVertex>()
    .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    .enableDepthTest()
    .enableBlending()         // optional
    .setPushConstantSize(sizeof(PushConstants))
    .build();
```

**Buffer** distinguishes between static and dynamic at creation time:
```cpp
auto terrainBuf = Buffer::createStatic(context, vertices);    // upload once
auto particleBuf = Buffer::createDynamic(context, maxSize);   // re-upload per frame
```

### scene/ — Renderable Objects

Each scene object knows how to build its geometry and record its draw commands into a command buffer. The Scene class manages draw ordering.

**Terrain** is the most complex object. It loads a rectangular patch from a NASA LOLA heightmap, generates a grid mesh with normals, and supports basic LOD (Level of Detail) — closer terrain patches use denser grids, distant ones use coarser grids. This keeps the triangle count manageable while preserving detail where the camera is looking.

The heightmap pipeline:
1. `fetch_terrain.py` downloads LOLA TIFF (64 ppd = ~474m/pixel at equator)
2. At startup, `Terrain` loads a cropped region (e.g., 512x512 patch around a landing site)
3. Each pixel becomes a vertex: X,Z from grid position, Y from elevation value
4. Normals computed from adjacent height differences (central differencing)
5. Index buffer connects grid into triangle pairs

**CelestialSphere** renders everything in the sky. Stars are rendered as point sprites. The Sun, Earth, and visible planets are positioned using SPICE ephemeris data. Earth renders as a small illuminated disc — even a 10-pixel circle with correct phase (lit portion based on Sun-Earth-Moon angle) adds tremendous immersion.

**Cockpit** is static geometry rendered last with depth testing disabled. It frames the view — struts, window edges, instrument console outline. Think Apollo LM: two triangular windows with metal framing. Renders on top of everything like a HUD layer, but with 3D perspective so the frame has parallax when you look around.

### sim/ — Pure Simulation

This module has zero Vulkan or rendering dependencies. It could be compiled and tested independently, which is important for open-source contributors who might not have a GPU.

**Physics** handles 6DOF rigid body dynamics:
- Position: `glm::dvec3` (double precision — orbital distances need it)
- Velocity: `glm::dvec3`
- Orientation: `glm::dquat` (quaternion — no gimbal lock)
- Angular velocity: `glm::dvec3`
- Thrust, gravity, torque applied each tick
- Collision detection against terrain heightmap

Why double precision? At orbital altitudes (~100km above the surface), single-precision floats give you ~1cm resolution for position, which sounds fine — but velocity integration accumulates error over time. Doubles give ~1 nanometer resolution, eliminating drift entirely. Downcast to float only when passing to the GPU.

**OrbitalMechanics** handles the transition from orbit to landing:
- Keplerian orbit computation (period, altitude, velocity at any point)
- Deorbit burn calculations (how much Δv to lower periapsis to the surface)
- Gravity turn guidance (the optimal trajectory from orbit to vertical descent)

This doesn't need to be a full N-body simulation. Two-body (Moon + lander) is sufficient — the Moon's gravity dominates at this range, and Earth/Sun perturbations are negligible for a landing sequence.

**Ephemeris** wraps NASA's CSPICE toolkit:
```cpp
// Get Earth's position as seen from the Moon at mission time
glm::dvec3 earthPos = Ephemeris::getBodyPosition("EARTH", "MOON", missionTime);

// Get Sun direction for lighting
glm::dvec3 sunDir = Ephemeris::getBodyDirection("SUN", "MOON", missionTime);
```

Required SPICE kernels:
- `de440.bsp` — planetary ephemeris (Sun, planets, Moon)
- `naif0012.tls` — leap seconds
- `pck00011.tpc` — planetary constants (radii, orientation)
- `moon_pa_de440_200625.bpc` — high-accuracy Moon orientation

These are ~100MB total, downloaded by `fetch_kernels.py` at build time.

**SimState** is the central state struct that bridges sim and rendering:
```cpp
struct SimState {
    // Lander
    glm::dvec3 position;        // meters, Moon-centered
    glm::dvec3 velocity;        // m/s
    glm::dquat orientation;     // quaternion
    glm::dvec3 angularVelocity; // rad/s
    float fuel;                 // kg
    bool thrusting;
    LandingState state;         // Orbit, Descent, Landed, Crashed

    // Derived (computed each frame for HUD)
    double altitude;            // above terrain
    double surfaceSpeed;        // horizontal component
    double verticalSpeed;       // vertical component
    double downrangeToTarget;   // distance to landing site
    float pitch, yaw, roll;    // Euler for display only

    // Environment
    glm::dvec3 sunDirection;
    glm::dvec3 earthPosition;
    double missionTime;         // seconds since epoch
};
```

### camera/ — View System

**Camera** modes:
1. **Cockpit** (default) — locked to lander, looking forward through the window. Orientation matches lander quaternion. Small head-look with mouse (±30°) for situational awareness.
2. **Chase** — follows behind and above the lander with smooth interpolation. Good for seeing the lander in context with terrain.
3. **Free** — detached, WASD + mouse fly-cam for debugging and cinematic screenshots.

### hud/ — Telemetry Display

The HUD is dense and informational — this is a simulation, not a game.

```
┌─────────────────────────────────────────────────────┐
│                                                     │
│  ALT  1,247 m  ▼ 12.3 m/s    MET 00:14:32.1      │
│  VEL  H: 3.2  V: -12.3  T: 12.7 m/s              │
│  ATT  P: -2.1° R: 0.4° Y: 12.0°                   │
│                                                     │
│           ┌───────────┐                             │
│           │     ●     │  ← attitude indicator       │
│           │   ──┼──   │    (artificial horizon)     │
│           │     │     │                             │
│           └───────────┘                             │
│                                                     │
│  FUEL ████████████░░░░  73%     ◇ ← target marker  │
│  THRT ████████░░░░░░░░  55%       (projected to    │
│  RNG  340m  BRG 045°              screen from 3D)  │
│                                                     │
│  ── trajectory arc ──                               │
│  (predicted flight path, drawn in world space       │
│   but visible through cockpit window)               │
│                                                     │
│  PHASE: POWERED DESCENT                             │
│  Σ ΔV REMAINING: 423 m/s                           │
│                                                     │
└─────────────────────────────────────────────────────┘
```

**Instruments** (screen-space):
- Altitude tape — numeric + rate arrow (▲/▼)
- Velocity vector — horizontal, vertical, total speed
- Attitude readout — pitch, roll, yaw in degrees
- Fuel bar + percentage + estimated burn time remaining
- Thrust level indicator
- Range and bearing to landing target
- Mission elapsed time (MET)
- Phase indicator (Orbit / Deorbit Burn / Powered Descent / Terminal / Landed)
- Total Δv remaining (computed from fuel mass + specific impulse)

**Attitude indicator** — a dynamic mesh rebuilt each frame showing horizon line rotated to match lander orientation, with trigonometric vertex placement for the circle and horizon.

**Trajectory prediction** — rendered in world space (not screen space). Forward-integrate the lander's current state 30–60 seconds into the future assuming current thrust, draw the predicted arc as a line strip. This lets the pilot see where they'll end up. Changes color from green (safe) to red (impact) based on predicted outcome.

**Landing target marker** — take the 3D world position of the landing pad, multiply by the current VP matrix, convert from NDC to screen coordinates, draw a diamond there. When aligned correctly, it appears centered in the cockpit window.

---

## Rendering Architecture

### Draw Order (Single Render Pass)

1. **Celestial sphere** — stars, planets, Sun, Earth (depth write OFF, always behind everything)
2. **Terrain** — heightmap mesh with normals, lit by Sun direction (depth write ON)
3. **Landing site markers** — geometry on the terrain surface
4. **Particles** — exhaust (blending ON, depth write OFF)
5. **Lander** — only visible in chase/free camera modes (depth write ON)
6. **Trajectory arc** — world-space line strip (blending ON, depth write OFF)
7. **Cockpit frame** — only in cockpit mode (depth test OFF, renders on top)
8. **HUD** — screen-space instruments (depth test OFF, own projection)

### Coordinate System

- **World origin:** Moon center
- **World units:** meters
- **World axes:** +X toward 0°N 0°E, +Y toward north pole, +Z toward 0°N 90°E (IAU Moon-fixed frame)
- **Lander local:** +X right, +Y up (thrust direction), +Z forward (out the window)

This matches the IAU_MOON reference frame used by SPICE, so ephemeris positions can be used directly.

### Depth Buffer

```cpp
// In RenderPass setup:
VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;  // 32-bit float depth

// Depth ranges are extreme in this sim:
//   Near plane: 0.1m (cockpit struts)
//   Far plane: 200,000m (200km, visible terrain from orbit)
//
// With D32_SFLOAT and reversed-Z (1.0 = near, 0.0 = far),
// precision is logarithmic — excellent near the camera,
// adequate at distance. Standard Z would give terrible
// near-plane precision at these ranges.
```

**Reversed-Z** is critical. With standard Z mapping, a 0.1m–200km range would give almost all depth precision to the far half. Reversed-Z flips this, giving the best precision close to the camera where it matters most (cockpit frame, terrain surface during landing).

---

## Data Sources

### Lunar Terrain — NASA LOLA

**Source:** CGI Moon Kit (https://svs.gsfc.nasa.gov/4720/)
- Displacement maps at 64, 16, and 4 pixels per degree
- Floating-point TIFF, values in kilometers relative to 1737.4 km reference sphere
- 64 ppd ≈ 474m per pixel at equator

**Higher resolution:** SLDEM2015 (https://pgda.gsfc.nasa.gov/products/54)
- 512 pixels per degree (≈ 60m resolution)
- Combined LOLA laser altimetry + SELENE stereo imagery
- Coverage: ±60° latitude

**Usage:**
1. Download global or regional tiles
2. Crop to landing site region (e.g., 10°x10° around Tranquility Base)
3. Load as heightmap, generate mesh vertices
4. Each pixel → (lon, lat, elevation) → (x, y, z) in Moon-fixed frame

### Celestial Positions — NASA SPICE

**Source:** NAIF (https://naif.jpl.nasa.gov/naif/toolkit.html)
- CSPICE toolkit (C library, compiles with CMake)
- DE440 planetary ephemeris — positions of all planets, Sun, Moon
- Accuracy: sub-kilometer for planets, sub-meter for Moon

**Usage:**
```cpp
// Initialize once
furnsh_c("de440.bsp");
furnsh_c("naif0012.tls");
furnsh_c("pck00011.tpc");

// Each frame: get Earth position relative to Moon
SpiceDouble et;
str2et_c("2026 FEB 11 12:00:00 UTC", &et);
SpiceDouble state[6], lt;
spkezr_c("EARTH", et, "IAU_MOON", "LT+S", "MOON", state, &lt);
// state[0..2] = position in km, state[3..5] = velocity in km/s
```

### Star Catalog

**Source:** Hipparcos catalog (public domain)
- ~118,000 stars with positions and magnitudes
- Filter to magnitude < 6.5 (naked eye visible) for ~9,000 stars
- Convert RA/Dec to 3D unit vectors on the celestial sphere

Procedural star placement on a unit sphere works as a starting point; replace with Hipparcos data later for accuracy.

---

## Build System

```cmake
cmake_minimum_required(VERSION 3.20)
project(luna3d LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)

# External dependencies
add_subdirectory(external/glfw)
add_subdirectory(external/glm)

# SPICE (C library)
add_library(cspice STATIC external/cspice/src/cspice/*.c)
target_include_directories(cspice PUBLIC external/cspice/include)

# Application modules
add_library(luna_core    STATIC src/core/*.cpp)
add_library(luna_scene   STATIC src/scene/*.cpp)
add_library(luna_sim     STATIC src/sim/*.cpp)
add_library(luna_input   STATIC src/input/*.cpp)
add_library(luna_camera  STATIC src/camera/*.cpp)
add_library(luna_hud     STATIC src/hud/*.cpp)
add_library(luna_util    STATIC src/util/*.cpp)

# Dependency graph:
#   luna_sim    → luna_util, cspice         (no Vulkan)
#   luna_core   → luna_util, Vulkan, GLFW
#   luna_scene  → luna_core, luna_util
#   luna_camera → luna_core, luna_util
#   luna_hud    → luna_core, luna_util
#   luna_input  → luna_util, GLFW
#   main        → everything

add_executable(luna3d src/main.cpp)
target_link_libraries(luna3d
    luna_core luna_scene luna_sim luna_input
    luna_camera luna_hud luna_util cspice
    Vulkan::Vulkan glfw glm)

# Shader compilation
file(GLOB SHADERS shaders/*.vert shaders/*.frag)
foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    add_custom_command(
        OUTPUT ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv
        COMMAND glslangValidator -V ${SHADER} -o ${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.spv
        DEPENDS ${SHADER})
endforeach()
```

---

## Implementation Phases

### Phase 1 — Core + Terrain

Build modular Vulkan infrastructure in `core/`. Load a LOLA heightmap patch and render as a lit terrain mesh with perspective camera. Depth buffer, basic directional lighting from a hardcoded sun direction. WASD free camera to fly around.

**Milestone:** Fly a camera over real lunar terrain.

### Phase 2 — Lander + Physics

Implement 6DOF physics with quaternion orientation. Load a simple lander OBJ mesh. Chase camera follows the lander. Collision detection against terrain heightmap. Thrust, rotation, gravity, fuel consumption.

**Milestone:** Fly and land a lander on real terrain in chase-cam view.

### Phase 3 — Cockpit + HUD

Build cockpit frame geometry. Implement first-person camera locked to lander orientation. Build the HUD system with altitude/velocity/attitude instruments. Landing target marker via world-to-screen projection.

**Milestone:** Land from cockpit view with telemetry readouts.

### Phase 4 — Celestial Bodies

Integrate CSPICE. Position Sun, Earth, and visible planets correctly for a given mission date/time. Sun direction drives terrain lighting. Earth renders as a small lit disc with correct phase. Star field from procedural or Hipparcos catalog.

**Milestone:** Looking up from the lunar surface, Earth and stars are in the right places.

### Phase 5 — Orbital Phase

Extend physics to handle orbital velocities. Start the player in a 100km circular orbit. Implement deorbit burn sequence. Terrain LOD so both orbital and surface views work. Trajectory prediction overlay.

**Milestone:** Complete mission: orbit → deorbit → powered descent → landing.

### Phase 6 — Polish + Open Source

Particle exhaust effects. NavBall attitude indicator. Configuration file for mission parameters (landing site, start orbit, date). README, build docs, contribution guidelines. GitHub release.

**Milestone:** Public repository, buildable on Linux/Windows, documented.

---

## Design Decisions

**Why not use an engine (Unreal, Godot)?**
Luna's value proposition is transparency. Every draw call, every matrix multiplication, every physics tick is visible in the source. An engine hides these behind abstractions. For learning Vulkan and simulation, the abstraction is the enemy.

**Why C++20?**
Designated initializers, concepts for template constraints on Vulkan wrappers, `std::span` for buffer views. Nothing exotic — just cleaner code than C++17 for this kind of systems work.

**Why SPICE instead of precomputed tables?**
SPICE is what NASA actually uses. It's the real tool. Contributors who learn SPICE here can apply it directly to mission planning tools. Also, it handles time systems (UTC, TDB, mission elapsed time) correctly, which is surprisingly hard to do from scratch.

**Why geometry first, textures later?**
Textures require descriptor sets, samplers, image views, and layout transitions — the most complex Vulkan boilerplate. By building everything with procedural shading first (terrain color from normals + height, lander from vertex colors), the project reaches a playable state faster. Textures are a future enhancement.

**Why double precision physics?**
At 100km orbital altitude, the Moon's radius (1,737,400m) plus altitude gives coordinates around 1,837,400. A 32-bit float has ~7 decimal digits of precision, giving ~0.1m resolution at this scale. That works for position, but velocity integration accumulates rounding errors over minutes of simulation. Doubles give ~15 digits, eliminating the problem entirely. The GPU still receives 32-bit floats — the downcast happens at the sim-to-render boundary.

---

## Naming Conventions

- Classes: `PascalCase` (VulkanContext, TerrainMesh, SimState)
- Functions: `camelCase` (updatePhysics, getProjectionMatrix)
- Members: `camelCase` (cameraPos, landerOrientation)
- Constants: `UPPER_SNAKE` (LUNAR_GRAVITY, MAX_PARTICLES)
- Files: `PascalCase.h/.cpp` matching class name
- Namespaces: `luna::core`, `luna::sim`, `luna::scene`, `luna::hud`
