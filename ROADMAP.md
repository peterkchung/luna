# Luna — Roadmap

Development phases for building a realistic lunar landing simulator. Each phase produces a runnable, visually meaningful result that builds on the previous.

---

## Completed

### Core Vulkan Infrastructure
- Vulkan context, swapchain, render pass, command pool, sync
- Pipeline builder with configurable topology, depth test, push constants
- Buffer management (static GPU-local with staging, dynamic host-visible)
- Shader compilation pipeline (GLSL → SPIR-V)
- RAII wrappers with move semantics throughout

### Terrain Rendering (Flat Patch)
- Procedural heightmap with layered noise and crater depressions
- Moon-centered coordinate system (IAU_MOON frame)
- Directional sun lighting with height-based coloring
- 256x256 vertex grid, ~10km coverage

### Camera System (Initial)
- Double-precision perspective camera with reversed-Z projection
- Free-look controller (WASD + mouse, scroll-wheel speed)

### Phase A — Camera-Relative Rendering
- [x] Add rotation-only view matrix to Camera (no translation baked in)
- [x] Update push constants: `mat4 viewProj` + `vec3 cameraOffset` + `vec4 sunDirection`
- [x] Update vertex shader to compute `viewPos = inPosition + cameraOffset`
- [x] Add `float height` vertex attribute for elevation-based coloring
- [x] Update fragment shader to use height attribute instead of `length(fragPosition)`

### Phase B — Cubesphere (Spherical Moon)
- [x] Extract heightmap sampling into `sim/TerrainQuery` (pure math, no Vulkan)
- [x] Implement cube-face-to-sphere projection (6 faces)
- [x] Implement `ChunkGenerator` — produces vertex/index data per patch
- [x] Implement `CubesphereBody` — manages 6 faces, each subdivided into patches
- [x] Store vertex positions relative to patch center (chunk-local coordinates)
- [x] ~~Apply procedural heightmap displacement on the sphere surface~~ (removed — flat terrain placeholder until NASA LOLA data)
- [x] Replace flat `Terrain` with `CubesphereBody` in main loop
- [x] Start camera at 100km altitude

### Phase C — Quadtree LOD
- [x] Implement quadtree node with split/merge state machine
- [x] Screen-space geometric error metric for LOD selection
- [x] Skirt geometry to fill T-junction gaps between LOD levels
- [x] Frustum culling (Gribb/Hartmann plane extraction, bounding sphere test)
- [x] Conservative bounding radius from corner/edge midpoint sampling
- [x] Mesh upload budget (max 32 splits per frame)
- [x] Batched GPU uploads via shared staging buffer (StagingBatch)
- [x] Deferred mesh destruction to prevent use-after-free during batched transfers

### Phase D — Physics Simulation
- [x] Implement `SimState` — position, velocity, orientation, fuel, flight phase
- [x] Implement `Physics` — semi-implicit Euler integration
- [x] Lunar gravity: `a = -GM/r^2 * normalize(position)` (GM = 4.9028695e12)
- [x] Thrust: body-frame +Y, transformed by orientation quaternion
- [x] Fuel consumption based on specific impulse
- [x] Terrain collision via `TerrainQuery` heightmap lookup
- [x] Landing/crash detection (vertical speed < 2 m/s, surface speed < 1 m/s)
- [x] Input mapping: throttle (Z/X), rotation torque (IJKL/UO)
- [x] Start in 100km circular orbit (v = ~1633 m/s prograde)

### Phase E — Camera Overhaul + Starfield
- [x] Refactor Camera to quaternion-based orientation (no Euler gimbal lock)
- [x] Radial "up" vector: `normalize(position)` instead of fixed `(0,1,0)`
- [x] Camera modes: free-fly and attached-to-lander (P to toggle)
- [x] Altitude-based movement speed scaling
- [x] Extend far plane to 2,000,000m (visible Moon limb from orbit)
- [x] Implement `Starfield` — ~5,000 procedural stars as point sprites
- [x] Starfield shaders with `gl_PointSize` and brightness variation
- [x] Pipeline builder additions: `setDepthWrite(bool)`, `enableAlphaBlending()`
- [x] Camera starts on -Y axis for natural Vulkan Y-down orientation

---

## Future

Features beyond the core simulation phases, in rough priority order.

### Cockpit + HUD
- Cockpit frame geometry (Apollo LM-style window framing)
- Altitude/velocity/attitude instruments
- Fuel gauge with estimated burn time
- Trajectory prediction overlay (forward-integrated arc)
- Landing target marker (world-to-screen projection)

### Celestial Bodies (CSPICE)
- NASA SPICE toolkit integration for real ephemeris data
- Sun positioned correctly for a given mission date/time
- Earth rendered as an illuminated disc with correct phase
- Star catalog (Hipparcos) replacing procedural stars

### Real Terrain Data
- NASA LOLA heightmap loading (TIFF → mesh)
- SLDEM2015 at 512 ppd (~60m resolution)
- Specific landing sites (Tranquility Base, Shackleton crater)

### Orbital Mechanics
- Keplerian orbit computation and visualization
- Deorbit burn calculations
- Gravity turn guidance
- Mission timeline (orbit → deorbit → powered descent → terminal → landing)

### Polish
- Particle exhaust effects
- NavBall attitude indicator
- Configuration file for mission parameters
- Windows/macOS build support
