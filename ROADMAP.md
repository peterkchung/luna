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

### Camera System
- Double-precision perspective camera with reversed-Z projection
- Free-look controller (WASD + mouse, scroll-wheel speed)
- Near=0.1m, far=200km depth range

---

## Phase A — Camera-Relative Rendering

**Goal:** Eliminate floating-point precision loss at Moon-scale coordinates.

- [ ] Add rotation-only view matrix to Camera (no translation baked in)
- [ ] Update push constants: `mat4 viewProj` + `vec3 cameraOffset` + `vec4 sunDirection`
- [ ] Update vertex shader to compute `viewPos = inPosition + cameraOffset`
- [ ] Add `float height` vertex attribute for elevation-based coloring
- [ ] Update fragment shader to use height attribute instead of `length(fragPosition)`

**Milestone:** Fly camera far from terrain with no visible jitter.

---

## Phase B — Cubesphere (Spherical Moon)

**Goal:** Replace the flat terrain patch with a full sphere visible from orbit.

- [ ] Extract heightmap sampling into `sim/TerrainQuery` (pure math, no Vulkan)
- [ ] Implement cube-face-to-sphere projection (6 faces)
- [ ] Implement `ChunkGenerator` — produces vertex/index data per patch
- [ ] Implement `CubesphereBody` — manages 6 faces, each subdivided into patches
- [ ] Store vertex positions relative to patch center (chunk-local coordinates)
- [ ] Apply procedural heightmap displacement on the sphere surface
- [ ] Replace flat `Terrain` with `CubesphereBody` in main loop
- [ ] Start camera at 100km altitude

**Milestone:** Full Moon sphere visible from orbit, heightmap detail visible.

---

## Phase C — Quadtree LOD

**Goal:** Smooth detail transition from orbit to surface.

- [ ] Implement `QuadtreeNode` with split/merge state machine
- [ ] Screen-space geometric error metric for LOD selection
- [ ] Edge stitching to prevent T-junction cracks (index buffer modification)
- [ ] Enforce max 1-level LOD difference between neighbors
- [ ] Frustum culling (test node bounding sphere against view frustum)
- [ ] Mesh upload budget (max 2–3 new meshes per frame to avoid stalls)

**Milestone:** Fly from 100km orbit to surface with smooth, crack-free LOD transitions.

**Constants:** `PATCH_GRID=33`, `MAX_DEPTH=15` (~2.6m vertex spacing at deepest), `MAX_ACTIVE_PATCHES=500`.

---

## Phase D — Physics Simulation

**Goal:** Orbital motion, gravity, thrust, and collision.

- [ ] Implement `SimState` — position, velocity, orientation, fuel, flight phase
- [ ] Implement `Physics` — semi-implicit Euler integration
- [ ] Lunar gravity: `a = -GM/r^2 * normalize(position)` (GM = 4.9028695e12)
- [ ] Thrust: body-frame +Y, transformed by orientation quaternion
- [ ] Fuel consumption based on specific impulse
- [ ] Terrain collision via `TerrainQuery` heightmap lookup
- [ ] Landing/crash detection (vertical speed < 2 m/s, surface speed < 1 m/s)
- [ ] Input mapping: throttle, rotation commands
- [ ] Start in 100km circular orbit (v = ~1633 m/s prograde)

**Milestone:** Lander orbits under gravity, thrust changes trajectory, can land or crash.

---

## Phase E — Camera Overhaul + Starfield

**Goal:** Proper orientation on a sphere and spatial context.

- [ ] Refactor Camera to quaternion-based orientation (no Euler gimbal lock)
- [ ] Radial "up" vector: `normalize(position)` instead of fixed `(0,1,0)`
- [ ] Camera modes: free-fly and attached-to-lander
- [ ] Altitude-based movement speed scaling
- [ ] Extend far plane to 2,000,000m (visible Moon limb from orbit)
- [ ] Implement `Starfield` — ~5,000 procedural stars as point sprites
- [ ] Starfield shaders with `gl_PointSize` and brightness variation
- [ ] Pipeline builder additions: `setDepthWrite(bool)`, `enablePointSize()`

**Milestone:** Stars visible in background, up stays radial to surface, camera tracks lander.

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
