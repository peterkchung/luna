# Luna — Roadmap

Development phases for building a realistic Artemis-era lunar landing simulator. The lander is modeled on SpaceX's Starship HLS (Human Landing System), selected by NASA for Artemis III and beyond. Each phase produces a runnable, visually meaningful result that builds on the previous.

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
- [x] Replace flat `Terrain` with `CubesphereBody` in main loop
- [x] Start camera at 100km altitude

### Phase C — Quadtree LOD
- [x] Implement quadtree node with split/merge state machine
- [x] Screen-space geometric error metric for LOD selection
- [x] Skirt geometry to fill T-junction gaps between LOD levels
- [x] Frustum culling (Gribb/Hartmann plane extraction, bounding sphere test)
- [x] Conservative bounding radius from corner/edge midpoint sampling
- [x] Mesh upload budget (max 64 splits per frame, breadth-first refinement)
- [x] Batched GPU uploads via shared staging buffer (StagingBatch)
- [x] Deferred mesh destruction to prevent use-after-free during batched transfers
- [x] Frustum-aware LOD — only split visible nodes
- [x] Distance-sorted child traversal for balanced budget distribution
- [x] Lazy staging batch allocation — zero Vulkan overhead when LOD is converged

### Phase D — Physics Simulation
- [x] Implement `SimState` — position, velocity, orientation, fuel, flight phase
- [x] Implement `Physics` — semi-implicit Euler integration
- [x] Lunar gravity: `a = -GM/r^2 * normalize(position)` (GM = 4.9028695e12)
- [x] Thrust: body-frame +Y via 2× Raptor Vacuum (4.4 MN max, Isp 380s)
- [x] Fuel consumption based on specific impulse
- [x] Terrain collision via `TerrainQuery` heightmap lookup
- [x] Landing/crash detection (vertical speed < 4 m/s, surface speed < 2 m/s)
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

### Phase F — Real Terrain Data
- [x] NASA LOLA heightmap loading (ldem_16.tif, 16 ppd equirectangular)
- [x] Minimal TIFF parser — no external library dependency
- [x] Bilinear interpolation with longitude wrapping
- [x] Vertex displacement by real elevation data
- [x] Central differencing normals from heightmap samples
- [x] Topographic contour lines (500m spacing, fwidth/smoothstep)
- [x] Additive bounding radius margin for terrain displacement (±12km)

### Phase G — HUD (Core Flight Instruments)
- [x] Seven-segment displays for altitude, vertical speed, surface speed
- [x] Bar gauges for throttle and fuel
- [x] Procedural rendering (no textures) — all push-constant driven
- [x] Screen-space overlay pipeline (alpha blend, no depth test)

### Phase G2 — HUD (Advanced Instruments + Cockpit Frame)
- [x] Cockpit frame (corner brackets, center crosshair)
- [x] Attitude indicator (pitch/roll horizon, pitch ladder, roll ticks)
- [x] Heading compass (scrolling tape with cardinal points)
- [x] Prograde marker (velocity vector screen projection)
- [x] Flight phase display (ORB/DSC/PWR/TRM/LND/FAIL)
- [x] Mission elapsed time (MM:SS seven-segment)
- [x] Time to surface countdown
- [x] Warning indicators (FUEL/RATE/TILT flashing)

### Infrastructure Fixes
- [x] Per-swapchain-image semaphores for correct resize handling
- [x] Bulk GPU release for fast cubesphere shutdown
- [x] Camera starts attached to lander, facing orbital velocity (+X)

---

## Future

Features beyond the core simulation phases, in priority order.

### HUD Phase 3 — Guidance Overlays
- Trajectory prediction overlay (forward-integrated arc)
- Landing target marker (world-to-screen projection)
- Fuel gauge with estimated burn time remaining

### NRHO Starting Orbit
- Near-Rectilinear Halo Orbit (~3,250 km perilune / ~71,000 km apolune)
- Transfer burn from NRHO to low lunar orbit
- Full Artemis III descent profile: NRHO → low orbit → powered descent

### Terrain Relative Navigation
- Starship HLS uses TRN + Lidar for safe landing site selection
- Hazard detection overlay showing slope/roughness analysis
- Automatic landing site targeting at lunar south pole

### Mid-Body Thruster Switchover
- Starship HLS switches from Raptor to CH4/LOX RCS at ~50m altitude
- Reduced thrust for dust mitigation during final descent
- Separate thruster model with lower Isp and thrust

### Higher Resolution Terrain
- SLDEM2015 at 512 ppd (~60m resolution)
- Specific landing sites (Shackleton crater rim, south pole regions)

### Celestial Bodies (CSPICE)
- NASA SPICE toolkit integration for real ephemeris data
- Sun positioned correctly for a given mission date/time
- Earth rendered as an illuminated disc with correct phase
- Star catalog (Hipparcos) replacing procedural stars

### Orbital Mechanics
- Keplerian orbit computation and visualization
- Deorbit burn calculations
- Gravity turn guidance
- Mission timeline (NRHO → transfer → powered descent → terminal → landing)

### Polish
- Particle exhaust effects (Raptor plume)
- Configuration file for mission parameters
- Windows/macOS build support
