# Luna

A first-person lunar landing simulator built from scratch in C++20 and Vulkan. No engine, no abstractions — every draw call, matrix multiplication, and physics tick is visible in the source.

## Objective

**What would it feel like to descend to the Moon from lunar orbit?**

Luna aims to answer this through geometric and physical fidelity: a spherical Moon with distance-based level of detail, 6DOF rigid body physics with real lunar gravity, and accurate spatial context. The focus is on representing the Moon as a realistic celestial body you can approach, orbit, and land on — not through textures, but through geometry, lighting, and physics.

## Current Status

**Working:**
- Vulkan rendering pipeline (RAII wrappers, reversed-Z depth, push constants)
- Cubesphere Moon with quadtree LOD (seamless orbital-to-surface detail)
- Skirt geometry to fill T-junction gaps between LOD levels
- Camera-relative rendering (double-precision offsets, no jitter at Moon scale)
- Frustum culling (Gribb/Hartmann plane extraction, bounding sphere test)
- Procedural terrain with layered noise and crater depressions
- Directional sun lighting with height-based surface coloring
- Quaternion camera with radial-up mouse look and altitude-scaled movement
- Procedural starfield (~5,000 point sprites)
- 6DOF rigid body physics (lunar gravity, thrust, fuel consumption, collision)
- Free-fly and lander-attached camera modes

**In progress:** See [ROADMAP.md](ROADMAP.md) for the full development plan.

## Controls

### Camera

| Input | Action |
|-------|--------|
| Right-click | Capture cursor for mouse look |
| ESC | Release cursor |
| W/A/S/D | Move forward/left/back/right |
| Q/E | Move down/up |
| Shift | 5x speed boost |
| Scroll wheel | Adjust base movement speed |

### Lander

| Input | Action |
|-------|--------|
| P | Toggle camera between free-fly and lander-attached |
| Z / X | Increase / decrease throttle |
| I / K | Pitch up / down |
| J / L | Yaw left / right |
| U / O | Roll left / right |

## Project Structure

```
luna/
├── src/
│   ├── main.cpp              # Entry point, render loop, input dispatch
│   ├── core/                 # Vulkan infrastructure (RAII wrappers)
│   ├── scene/                # Renderable objects
│   │   ├── CubesphereBody    # Spherical Moon: 6-face quadtree LOD
│   │   ├── ChunkGenerator    # Per-patch mesh generation
│   │   ├── Starfield         # Procedural star point cloud
│   │   └── Mesh              # Vertex/index buffer pair
│   ├── sim/                  # Simulation (no Vulkan dependency)
│   │   ├── SimState          # Position, velocity, orientation, fuel
│   │   ├── Physics           # 6DOF rigid body, gravity, thrust
│   │   └── TerrainQuery      # Heightmap sampling (pure math)
│   ├── input/                # GLFW input handling
│   ├── camera/               # Quaternion camera + controller
│   └── util/                 # Math helpers, logging, file I/O
├── shaders/                  # GLSL vertex/fragment shaders → SPIR-V
├── DESIGN.md                 # Architecture and module design
├── ROADMAP.md                # Development phases and progress
└── CHANGELOG.md              # Version history
```

See [DESIGN.md](DESIGN.md) for detailed module responsibilities, rendering architecture, and design decisions.

## Development Setup

### Prerequisites

- C++20 compiler (GCC 12+, Clang 14+, MSVC 2022+)
- CMake 3.20+
- Vulkan SDK 1.3+ (includes `glslangValidator` for shader compilation)
- GLFW 3.x and GLM (system packages or vcpkg)

### Build

```bash
git clone https://github.com/<user>/luna.git
cd luna

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run

```bash
./build/luna3d
```

## Architecture Overview

Luna uses a modular library design where each subsystem is a static library with explicit dependencies:

```
luna_sim    → luna_util              (no Vulkan — testable without GPU)
luna_core   → luna_util, Vulkan, GLFW
luna_scene  → luna_core, luna_sim, luna_util
luna_camera → luna_util
luna_input  → luna_util, GLFW
luna3d      → all of the above
```

Key architectural decisions:
- **Double precision** for all simulation-scale coordinates (positions, velocities), downcast to float only at the GPU boundary
- **Camera-relative rendering** to eliminate floating-point jitter at Moon-scale distances (~1.7M meters)
- **Cubesphere terrain** with quadtree LOD for seamless orbital-to-surface detail
- **Skirt geometry** on patch edges to fill T-junction gaps between LOD levels
- **Reversed-Z depth buffer** (D32_SFLOAT) for precision across 0.5m–2,000km range
- **Push constants only** (no descriptor sets yet) — keeps the Vulkan surface area minimal
- **Camera starts on -Y axis** where Vulkan's Y-down convention naturally places the Moon at the screen bottom with no orientation workarounds

## License

Apache 2.0
