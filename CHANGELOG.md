# Changelog

All notable changes to Luna are documented here, grouped by development phase.

---

## Performance + Visual Refinement

### 437df82 — style: brighten terrain and add lat/lon gridlines
- Increase base terrain color from dark gray to lighter gray (vec3(0.6))
- Raise ambient lighting from 0.05 to 0.12
- Add 10-degree lat/lon grid lines via sphere direction in fragment shader
- Pass sphere direction as varying from vertex shader for stable fwidth derivatives

### 7027dbb — perf: batch GPU uploads with shared staging buffer and deferred mesh destruction
- Add StagingBatch bump allocator: single host-visible buffer for all staging copies in a batch
- Update Buffer::createStaticBatch to suballocate from StagingBatch instead of per-call vkAllocateMemory
- Update Mesh batched constructor to use StagingBatch
- CubesphereBody constructor and update() now batch all transfers into one command buffer submission
- Defer mesh destruction during LOD splits/merges to prevent use-after-free on VRAM buffers still referenced by in-flight transfer commands

### 33471e8 — revert: remove batched uploads to isolate GPUVM fault cause

### 003c451 — revert: remove octahedron vertex encoding causing GPUVM fault on RADV

### 0215f98 — perf: tune LOD constants — smaller patches, less aggressive splitting
- PATCH_GRID: 33 → 17, SPLIT_THRESHOLD: 2.0 → 4.0, MERGE_THRESHOLD: 1.0 → 2.0, MAX_SPLITS_PER_FRAME: 64 → 32

### 5c5f553 — perf: flatten terrain, remove procedural noise and central differencing
- TerrainQuery::sampleTerrainHeight returns 0 (flat terrain placeholder for NASA LOLA data)
- ChunkGenerator uses sphere normals directly, removes central differencing and heightmap sampling

---

## Phase E — Camera Overhaul + Starfield

### c974720 — fix: camera on -Y axis, skirt geometry, faster LOD convergence
- Move camera start position to -Y axis where Vulkan's Y-down convention naturally places the Moon at the screen bottom with no roll or viewport workaround
- Add skirt geometry to patch edges to fill T-junction gaps between LOD levels
- Increase split budget from 8 to 64 per frame for near-instant LOD convergence
- Reduce mouse sensitivity from 0.002 to 0.001 rad/pixel
- Compute bounding radius from actual corner/edge midpoint distances with 5% margin

### ed4f6b1 — fix: correct face 0/1 winding order for consistent back-face culling
- Swap u,v mapping for cube faces 0 (+X) and 1 (-X) so cross(dP/du, dP/dv) points outward
- Verified all 6 faces produce correct CCW winding for front-face convention

### f9799c7 — fix: output compiled shaders to source dir, add pipeline builder options
- Change CMake shader output from build/ to source shaders/ directory so the binary finds them when run from project root
- Add `setCullMode()` and `setDepthWrite()` to Pipeline builder

### 374d768 — feat: quaternion camera, starfield, and pipeline alpha/depth-write controls
- Refactor Camera from Euler angles to quaternion orientation (no gimbal lock)
- Add `rotate()` with arbitrary world-up axis for radial mouse look on a sphere
- Add `localUp()` returning `normalize(position)` for radial direction
- Implement CameraController with radial-up yaw, altitude-scaled movement speed
- Add Starfield (~5,000 procedural stars as point sprites)
- Add starfield vertex/fragment shaders with `gl_PointSize` and brightness
- Add `enableAlphaBlending()` to Pipeline builder
- Extend camera far plane to 2,000,000m

## Phase D — Physics Simulation

### 0f1849f — feat: add 6DOF physics with lunar gravity, thrust, and collision
- Create SimState with position, velocity, quaternion orientation, fuel, flight phase
- Implement Physics with semi-implicit Euler integration
- Lunar gravity: GM = 4.9028695e12 m^3/s^2
- Thrust along body-frame +Y, transformed by orientation quaternion
- Fuel consumption based on specific impulse (311s) and throttle
- Terrain collision detection via heightmap lookup
- Landing criteria: vertical < 2 m/s, horizontal < 1 m/s
- Wire controls: Z/X throttle, IJKL pitch/yaw, UO roll, P camera toggle

## Phase C — Quadtree LOD

### 3498174 — feat: add quadtree LOD with frustum culling to cubesphere
- Replace fixed subdivision with dynamic quadtree (6 root nodes, one per face)
- Screen-space geometric error metric drives split/merge decisions
- Frustum culling via Gribb/Hartmann plane extraction and bounding sphere test
- Split budget prevents GPU stalls from too many mesh uploads per frame
- Constants: PATCH_GRID=33, MAX_DEPTH=15, SPLIT_THRESHOLD=2.0, MERGE_THRESHOLD=1.0

## Phase B — Cubesphere (Spherical Moon)

### db08ba9 — feat: implement cubesphere Moon with camera-relative rendering
- Extract heightmap noise into sim/TerrainQuery (no Vulkan dependency)
- Implement ChunkGenerator: cube-face-to-sphere projection with heightmap displacement
- Implement CubesphereBody: manages 6 faces of terrain patches
- Camera-relative rendering: rotation-only VP matrix + per-chunk double-precision offset
- Push constants: mat4 viewProj + vec3 cameraOffset + vec4 sunDirection (96 bytes)
- Vertex positions stored relative to chunk center for float precision
- Add height vertex attribute for elevation-based terrain coloring
- Update terrain shaders for camera-relative transform
- Add LUNAR_GM constant and dquat type alias
- Camera starts at 100km altitude above the surface

## Phase A — Core Infrastructure

### f75e052 — docs: update design and readme, add roadmap
- Add DESIGN.md with full architecture documentation
- Add ROADMAP.md with phased development plan
- Update README.md with project structure and build instructions

### 4f636e3 — feat: integrate terrain rendering with camera and lighting
- Connect terrain mesh, camera, and directional lighting in the render loop

### 669d755 — feat(scene): add terrain mesh generation
- Procedural terrain with layered noise and crater depressions
- Height-based surface coloring

### 7e8dbd4 — feat: add terrain shaders with directional lighting
- Terrain vertex/fragment shaders with sun direction push constant

### f256664 — feat(camera): add perspective camera with free-look controls
- Double-precision perspective camera with reversed-Z projection
- WASD + mouse look, scroll-wheel speed control

### 1e5a4c1 — feat(input): add keyboard and mouse input manager
- GLFW key/mouse polling with cursor capture

### 04c4a28 — feat: render test triangle to validate pipeline
- First rendered frame — triangle to verify the full Vulkan pipeline

### 2677070 — feat(core): add shader module, pipeline builder, and buffer management
- SPIR-V shader loading, configurable pipeline builder, static/dynamic buffers

### 13f9f02 — feat(core): add swapchain, render pass, and frame sync
- Swapchain with recreation, render pass with reversed-Z depth, frame synchronization

### 429acc8 — feat(core): add Vulkan instance, device, and window surface
- VulkanContext RAII wrapper: instance, physical/logical device, queues, GLFW window

### 5cd5941 — build: add CMakeLists.txt and project skeleton
- CMake build system with modular static libraries

### 5462624 — chore: update gitignore, commit design doc, add license
- Apache 2.0 license, initial .gitignore

### 1f92e5b — init commit
- Repository initialization
