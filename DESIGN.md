# Luna — Design Document

Technical reference for Luna's architecture, module design, rendering pipeline, and data sources.

---

## Project Structure

Files marked with `*` are implemented. Unmarked files are planned.

```
luna/
├── CMakeLists.txt *
├── LICENSE *
├── README.md *
├── DESIGN.md *
├── ROADMAP.md *
├── CHANGELOG.md *
│
├── assets/
│   ├── terrain/                # NASA LOLA heightmaps
│   ├── ephemeris/              # SPICE kernels (future)
│   └── models/                 # OBJ meshes (future)
│       ├── cockpit.obj
│       └── starship-hls.obj
│
├── shaders/
│   ├── terrain.vert *          # Terrain mesh vertex shader (camera-relative)
│   ├── terrain.frag *          # Terrain mesh fragment shader
│   ├── starfield.vert *        # Star point-sprite vertex shader
│   ├── starfield.frag *        # Star point-sprite fragment shader
│   ├── cockpit.vert            # Cockpit frame geometry (future)
│   ├── cockpit.frag
│   ├── particle.vert           # Engine exhaust particles (future)
│   ├── particle.frag
│   ├── hud.vert *              # Screen-space HUD overlay
│   ├── hud.frag *
│   └── trajectory.vert/.frag   # Predicted flight path overlay (future)
│
├── src/
│   ├── main.cpp *              # Entry point, app lifecycle
│   │
│   ├── core/ *                 # Vulkan infrastructure (all implemented)
│   │   ├── VulkanContext.h/cpp      # Instance, device, queues, window
│   │   ├── Swapchain.h/cpp          # Swapchain + recreation
│   │   ├── RenderPass.h/cpp         # Render pass + reversed-Z depth
│   │   ├── Pipeline.h/cpp           # Pipeline builder (configurable)
│   │   ├── Buffer.h/cpp             # Static GPU-local + dynamic host-visible
│   │   ├── CommandPool.h/cpp        # Command buffer management
│   │   ├── ShaderModule.h/cpp       # SPIR-V loading
│   │   ├── Image.h/cpp              # Image/texture creation + views
│   │   └── Sync.h/cpp               # Fences, per-image semaphores, frame sync
│   │
│   ├── scene/ *                # Scene objects and management
│   │   ├── Mesh.h/cpp *             # Vertex/index buffer pair + draw
│   │   ├── CubesphereBody.h/cpp *   # Spherical Moon: 6-face quadtree LOD
│   │   ├── ChunkGenerator.h/cpp *   # Generates vertex/index data per patch
│   │   ├── Starfield.h/cpp *        # Procedural star point cloud
│   │   ├── CelestialSphere.h/cpp    # Stars, planets, sun, earth (future)
│   │   ├── ParticleSystem.h/cpp     # Exhaust particles (future)
│   │   └── Cockpit.h/cpp            # Static cockpit frame geometry (future)
│   │
│   ├── sim/ *                  # Simulation (no Vulkan dependencies)
│   │   ├── SimState.h *             # Central simulation state
│   │   ├── Physics.h/cpp *          # 6DOF rigid body, gravity, thrust
│   │   ├── TerrainQuery.h/cpp *     # Heightmap sampling (pure math)
│   │   ├── OrbitalMechanics.h/cpp   # Keplerian orbits (future)
│   │   ├── Ephemeris.h/cpp          # SPICE wrapper (future)
│   │   └── TimeManager.h/cpp        # Mission elapsed time (future)
│   │
│   ├── input/ *                # Input handling
│   │   ├── InputManager.h/cpp *     # GLFW key/mouse polling
│   │   └── FlightControls.h/cpp     # Maps inputs to sim commands (future)
│   │
│   ├── camera/ *               # Camera system
│   │   ├── Camera.h/cpp *           # Quaternion camera, reversed-Z projection
│   │   └── CameraController.h/cpp * # Radial-up mouse look, altitude-scaled speed
│   │
│   ├── hud/ *                  # Heads-up display
│   │   └── Hud.h/cpp *              # Flight instruments, attitude, cockpit frame
│   │
│   └── util/ *                 # Shared utilities
│       ├── Math.h *                 # GLM config, double-precision types, constants
│       ├── FileIO.h/cpp *           # File reading, path resolution
│       └── Log.h/cpp *              # Lightweight logging
│
└── tools/                      # Data retrieval scripts (future)
    ├── fetch_terrain.py
    └── fetch_kernels.py
```

---

## Module Dependencies

```
luna_sim    → luna_util              (no Vulkan — testable without GPU)
luna_core   → luna_util, Vulkan, GLFW
luna_scene  → luna_core, luna_sim, luna_util
luna_camera → luna_util
luna_input  → luna_util, GLFW
main        → everything
```

The key property: `luna_sim` has no Vulkan dependency. Physics can be unit tested without a GPU, contributors can work on simulation logic on any machine, and the sim module could be reused in a headless environment.

---

## Module Responsibilities

### core/ — Vulkan Infrastructure

Thin RAII wrappers around Vulkan objects. Each class owns its resources and cleans up in its destructor. No rendering logic — just resource management.

**VulkanContext** owns the instance, physical device, logical device, and queues. Everything else receives a reference to it.

**Pipeline** uses a builder pattern:
```cpp
auto pipeline = Pipeline::Builder(context, renderPass)
    .setShaders("terrain.vert.spv", "terrain.frag.spv")
    .setVertexBinding(sizeof(ChunkVertex), { ... })
    .setTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    .enableDepthTest()
    .setPushConstantSize(sizeof(TerrainPushConstants))
    .build();
```

**Buffer** distinguishes between static and dynamic at creation time:
```cpp
auto terrainBuf = Buffer::createStatic(ctx, cmdPool, usage, data, size); // GPU-local, staged, blocking
auto particleBuf = Buffer::createDynamic(ctx, usage, size);              // host-visible, mappable
```

**StagingBatch** is a bump allocator over a single host-visible buffer, used to batch multiple GPU uploads into one command buffer submission. This avoids per-mesh `vkQueueWaitIdle` stalls during LOD splits:
```cpp
StagingBatch staging;
staging.begin(ctx, totalBytes);           // allocate + map one staging buffer
auto buf = Buffer::createStaticBatch(ctx, cmd, usage, data, size, staging); // suballocate + record copy
staging.end();                            // unmap
cmdPool.endOneShot(cmd, queue);           // submit all copies at once
```
Meshes replaced during batched uploads are deferred in `deferredDestroy_` until after the transfer command buffer completes, preventing use-after-free on VRAM buffers still referenced by in-flight copy commands.

**Sync** manages per-frame fences (`MAX_FRAMES_IN_FLIGHT = 2`) and per-swapchain-image semaphores. Semaphores are sized to the swapchain image count (typically 3–4) rather than to `MAX_FRAMES_IN_FLIGHT`, because the presentation engine holds a semaphore until its image is re-acquired — independent of fence state. A separate `currentSemaphore` index cycles through the image count. Semaphores are destroyed and recreated on swapchain recreation.

### scene/ — Renderable Objects

Each scene object builds its geometry and records draw commands into a command buffer.

**CubesphereBody** is the core terrain system. It represents the Moon as a sphere built from 6 cube faces projected onto a sphere surface. Each face is a quadtree of patches that subdivide based on camera distance, giving high detail near the surface and a smooth sphere from orbit.

The cubesphere pipeline:
1. 6 cube faces, each the root of a quadtree
2. Each quadtree node covers a UV region on its face
3. Node UV coordinates are projected onto the unit sphere, then scaled to `LUNAR_RADIUS`
4. Terrain displaced by NASA LOLA heightmap (16 ppd equirectangular TIFF)
5. Vertex positions are stored **relative to the patch center** (preserves float precision)
6. LOD selection: nodes split/merge based on screen-space geometric error
7. Skirt geometry fills T-junction gaps between LOD levels

**ChunkGenerator** produces vertex/index data for a single quadtree patch:
- Projects cube-face UV grid onto sphere surface
- Vertices displaced by LOLA heightmap, normals computed via central differencing
- Stores positions relative to patch center (`dvec3` center, `vec3` local offsets)
- Generates skirt geometry on all 4 edges to fill T-junction gaps

**Starfield** renders ~5,000 procedural stars as point sprites. Positions are unit direction vectors on a conceptual sphere. Rendered before terrain with depth write OFF.

### hud/ — Heads-Up Display

**Hud** renders screen-space flight instruments as procedural geometry (no textures). All rendering is driven by push constants containing telemetry from `SimState`. The fragment shader implements each instrument type via an `instrumentId` selector:

- **Seven-segment displays** — altitude, vertical speed, surface speed, mission elapsed time, time to surface
- **Bar gauges** — throttle level, fuel fraction
- **Attitude indicator** — pitch/roll horizon with pitch ladder and roll ticks
- **Heading compass** — scrolling tape with cardinal points (N/E/S/W)
- **Prograde marker** — velocity vector projected to screen coordinates
- **Cockpit frame** — corner brackets, center crosshair
- **Warning indicators** — FUEL/RATE/TILT with flashing
- **Flight phase** — ORB/DSC/PWR/TRM/LND/FAIL text display

The HUD pipeline uses alpha blending, no depth test, and `VK_CULL_MODE_NONE`. Panel geometry is a flat quad mesh with screen-space UV coordinates, rendered after all world geometry.

### sim/ — Pure Simulation

Zero Vulkan or rendering dependencies. Can be compiled and tested independently.

**SimState** is the central state struct:
```cpp
struct SimState {
    glm::dvec3 position;           // meters, Moon-centered
    glm::dvec3 velocity;           // m/s
    glm::dquat orientation;        // body-to-world quaternion
    glm::dvec3 angularVelocity;    // rad/s, body frame

    double dryMass = 85000.0;       // kg (Starship HLS)
    double fuelMass = 200000.0;     // kg (CH4/LOX)
    double specificImpulse = 380.0; // seconds (Raptor Vacuum)
    double maxThrust = 4400000.0;   // Newtons (2x Raptor Vacuum)
    double throttle = 0.0;
    glm::dvec3 torqueInput{0.0};

    FlightPhase phase;             // Orbit, Descent, Landed, Crashed
    double altitude;               // above terrain surface
    double surfaceSpeed;           // horizontal component
    double verticalSpeed;          // radial component
    double missionTime;            // seconds since start
};
```

**Physics** handles 6DOF rigid body dynamics with semi-implicit Euler integration:
- Gravity: `a = -GM/r^2 * normalize(position)` where `GM = 4.9028695e12 m^3/s^2`
- Thrust: body-frame +Y transformed to world via orientation quaternion (2× Raptor Vacuum)
- Fuel consumption: `dm/dt = throttle * maxThrust / (Isp * g0)`
- Collision detection against terrain heightmap
- Landing criteria: vertical speed < 4 m/s, surface speed < 2 m/s

Why double precision? At orbital altitude (~100km), Moon-centered coordinates are ~1,837,400m. A 32-bit float gives ~0.1m resolution — acceptable for position, but velocity integration accumulates rounding error over minutes. Doubles give ~15 digits of precision, eliminating drift. Downcast to float only at the sim-to-render boundary.

**TerrainQuery** provides heightmap sampling as a pure function, shared by both mesh generation (scene/) and collision detection (sim/). Loads NASA LOLA elevation data at startup; falls back to flat terrain if the TIFF is missing.

### camera/ — View System

**Camera** uses double-precision quaternion orientation:
- Position: `dvec3` (Moon-centered)
- Orientation: `dquat` (no gimbal lock at poles)
- Local up: `normalize(position)` — radial from Moon center
- `getRotationOnlyViewMatrix()` — rotation without translation (for camera-relative rendering)
- `getProjectionMatrix()` — reversed-Z, near=0.5m, far=2,000,000m

The camera starts on the **-Y axis** at 100km altitude, **facing orbital velocity (+X)**. This is deliberate: Vulkan's clip space has +Y pointing downward on screen. By positioning the camera on -Y, its default up (+Y) points toward the Moon center, which Vulkan renders at the screen bottom. Moon below, stars above — no viewport flip or roll correction needed. The initial yaw of -90° aligns the view with the prograde direction, giving an immediate sense of orbital motion.

**CameraController** modes:
1. **Free-fly** — WASD + mouse look with radial up vector. Speed scales with altitude.
2. **Attached** — locked to lander position/orientation from SimState (toggle with P key).

---

## Rendering Architecture

### Camera-Relative Rendering

At Moon scale, world-space coordinates (~1.7M meters) exceed float precision. Camera-relative rendering solves this by keeping all GPU-side math in a small coordinate frame near the camera.

**Data flow:**
```
CPU (double precision)                    GPU (float precision)
========================                  =====================

camera.position_ (dvec3)  --|
                             |-->  offset = vec3(chunkCenter - camPos)
chunk.worldCenter (dvec3) --|                      |
                                                   v
camera rotation   (dmat4) --|              push_constant {
camera projection (dmat4) --|--> viewProj    mat4 viewProj      (64 bytes)
                                             vec3 cameraOffset   (12 bytes)
                                             float _pad          ( 4 bytes)
                                             vec4 sunDirection   (16 bytes)
                                           }                    = 96 bytes

Vertex shader:
  vec3 viewPos = inPosition + pc.cameraOffset;
  gl_Position = pc.viewProj * vec4(viewPos, 1.0);
```

Vertex positions in each chunk are relative to the chunk center (~50m max magnitude at deepest LOD). The chunk-to-camera offset is computed in double precision on the CPU, then passed as a float vec3 (safe because relative distances between camera and nearby chunks are small). The view-projection matrix contains only rotation and projection — no translation.

### Draw Order (Single Render Pass)

1. **Starfield** — point sprites (depth write OFF, always behind everything)
2. **Terrain (CubesphereBody)** — per-chunk push constants, camera-relative (depth write ON)
3. **Particles** — exhaust (blending ON, depth write OFF) — future
4. **Lander** — only in chase/free mode (depth write ON) — future
5. **Cockpit frame** — cockpit mode only (depth test OFF, renders on top) — future
6. **HUD** — screen-space instruments (depth test OFF, alpha blending, push-constant driven)

### Coordinate System

- **World origin:** Moon center
- **World units:** meters
- **World axes:** +X toward 0°N 0°E, +Y toward north pole, +Z toward 0°N 90°E (IAU_MOON frame)
- **Camera start:** -Y axis (100km above surface), facing +X (orbital velocity), Vulkan Y-down aligns Moon below
- **Lander local:** +X right, +Y up (Raptor thrust direction), +Z forward
- **Chunk local:** positions relative to chunk center on the sphere surface

### Depth Buffer

```cpp
VkFormat depthFormat = VK_FORMAT_D32_SFLOAT;  // 32-bit float depth

// Near plane: 0.5m (terrain surface during landing)
// Far plane: 2,000,000m (2000km, visible Moon limb from orbit)
//
// Reversed-Z (1.0 = near, 0.0 = far) gives logarithmic precision
// distribution — excellent near the camera where it matters most.
```

---

## Cubesphere Terrain System

### Geometry

A cubesphere starts with 6 faces of a unit cube. Each vertex on a face is normalized to project onto the unit sphere, then scaled by `LUNAR_RADIUS + heightmapDisplacement`:

```
Cube face projection (6 faces):
+X: (1, u, v)     -X: (-1, -u, v)
+Y: (u, 1, -v)    -Y: (u, -1, v)
+Z: (u, v, 1)     -Z: (-u, v, -1)

where u, v range from -1 to +1 across the face.
```

### Quadtree LOD

Each face is the root of a quadtree. Nodes subdivide when the camera is close enough that geometric error exceeds a screen-space threshold:

```
geometricError = nodeArcLength / gridResolution
distance = length(nodeCenter - cameraPosition)     // double precision
screenError = geometricError / distance * screenFactor

Split when screenError > 4.0 pixels
Merge when all children < 2.0 pixels
```

| Depth | Patches/face | Patch edge length | Vertex spacing (17x17) |
|-------|-------------|-------------------|----------------------|
| 0     | 1           | ~2,730 km         | ~170 km              |
| 4     | 256         | ~170 km           | ~10.7 km             |
| 8     | 65,536      | ~10.7 km          | ~668 m               |
| 12    | 16.7M       | ~670 m            | ~42 m                |
| 15    | 1.07B       | ~83 m             | ~5.2 m               |

In practice, only ~50–200 patches are active at any time (deep only near camera). At 17x17 vertices (512 triangles per patch + skirts), 200 patches = ~120k triangles.

### Skirt Geometry

Each patch has a "skirt" — a strip of triangles hanging inward (toward the Moon center) from every edge. This fills T-junction gaps that occur when adjacent patches are at different LOD levels. The skirt depth is proportional to the patch's geometric error (one grid cell's arc length), ensuring gaps are always covered regardless of the LOD difference between neighbors.

### Chunk Vertex Format

```cpp
struct ChunkVertex {
    glm::vec3 position;   // relative to chunk center (small values, good precision)
    glm::vec3 normal;     // surface normal
    float     height;     // elevation above LUNAR_RADIUS in meters
};
```

---

## Data Sources

### Lunar Terrain — NASA LOLA

**Source:** CGI Moon Kit (https://svs.gsfc.nasa.gov/4720/)
- Displacement maps at 64, 16, and 4 pixels per degree
- Floating-point TIFF, values in kilometers relative to 1737.4 km reference sphere

**Higher resolution:** SLDEM2015 (https://pgda.gsfc.nasa.gov/products/54)
- 512 pixels per degree (~60m resolution)
- Combined LOLA laser altimetry + SELENE stereo imagery

### Celestial Positions — NASA SPICE (Future)

**Source:** NAIF (https://naif.jpl.nasa.gov/naif/toolkit.html)
- DE440 planetary ephemeris — positions of all planets, Sun, Moon
- Required kernels: `de440.bsp`, `naif0012.tls`, `pck00011.tpc`, `moon_pa_de440_200625.bpc`

### Star Catalog

Procedural placement initially. Future: Hipparcos catalog (~9,000 naked-eye stars).

---

## Design Decisions

**Why not use an engine (Unreal, Godot)?**
Luna's value proposition is transparency. Every draw call, every matrix multiplication, every physics tick is visible in the source. An engine hides these behind abstractions.

**Why C++20?**
Designated initializers, concepts for template constraints on Vulkan wrappers, `std::span` for buffer views. Cleaner code than C++17 for systems work.

**Why a cubesphere instead of an icosphere?**
Cubespheres have a natural UV parameterization (each face is a 2D grid), making quadtree subdivision, texture mapping, and neighbor-finding straightforward. Icospheres have more uniform triangle distribution but complex adjacency. For a terrain system with LOD, the cubesphere's regularity wins.

**Why camera-relative rendering?**
At Moon scale (~1.7M meters from origin), 32-bit float vertex positions have ~0.25m quantization error. Camera-relative rendering keeps vertex math in a small coordinate frame near the camera, preserving full float precision where it matters. The large-scale offset is computed in double precision on the CPU.

**Why geometry first, textures later?**
Textures require descriptor sets, samplers, image views, and layout transitions — the most complex Vulkan boilerplate. Building everything with procedural shading first (terrain color from normals + elevation) reaches a functional state faster. Textures are a future enhancement.

**Why double precision physics?**
At 100km orbital altitude, coordinates are ~1,837,400m. A 32-bit float has ~7 decimal digits, giving ~0.1m resolution. Velocity integration accumulates rounding errors over minutes. Doubles give ~15 digits, eliminating drift entirely. The GPU receives 32-bit floats — downcast happens at the sim-to-render boundary.

**Why semi-implicit Euler instead of RK4?**
For a real-time landing sim with frame-rate-coupled physics, semi-implicit Euler is energy-preserving enough for orbits and much simpler to implement. RK4 can be swapped in later if orbit stability over long durations becomes important.

---

## Naming Conventions

- Classes: `PascalCase` (VulkanContext, CubesphereBody, SimState)
- Functions: `camelCase` (updatePhysics, getProjectionMatrix)
- Members: `camelCase` with trailing underscore (position_, fuelMass_)
- Constants: `UPPER_SNAKE` (LUNAR_RADIUS, LUNAR_GM, MAX_DEPTH)
- Files: `PascalCase.h/.cpp` matching class name
- Namespaces: `luna::core`, `luna::sim`, `luna::scene`, `luna::camera`
