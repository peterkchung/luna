# Luna

A first-person lunar landing simulator built from scratch in C++20 and Vulkan. No engine, no abstractions — every draw call, matrix multiplication, and physics tick is visible in the source.

## Objective

**What would it feel like to descend to the Moon from lunar orbit?**

Luna aims to answer this through geometric and physical fidelity: a spherical Moon with distance-based level of detail, 6DOF rigid body physics with real lunar gravity, and accurate spatial context. The focus is on representing the Moon as a realistic celestial body you can approach, orbit, and land on — not through textures, but through geometry, lighting, and physics.

## Current Status

**Working:**
- Vulkan rendering pipeline (RAII wrappers, reversed-Z depth, push constants)
- Procedural terrain mesh with directional lighting
- Double-precision perspective camera with free-look controls
- WASD movement with scroll-wheel speed control (1 m/s – 100 km/s)

**In progress:** See [ROADMAP.md](ROADMAP.md) for the full development plan.

## Controls

| Input | Action |
|-------|--------|
| Right-click | Capture cursor for mouse look |
| ESC | Release cursor |
| W/A/S/D | Move forward/left/back/right |
| Q/E | Move down/up |
| Shift | 5x speed boost |
| Scroll wheel | Adjust base movement speed |

## Project Structure

```
luna/
├── src/
│   ├── main.cpp          # Entry point and render loop
│   ├── core/             # Vulkan infrastructure (RAII wrappers)
│   ├── scene/            # Renderable objects (terrain, mesh)
│   ├── sim/              # Simulation engine (no Vulkan dependency)
│   ├── input/            # GLFW input handling
│   ├── camera/           # Double-precision perspective camera
│   └── util/             # Math helpers, logging, file I/O
├── shaders/              # GLSL vertex/fragment shaders → SPIR-V
├── assets/               # Terrain data, models (future)
├── DESIGN.md             # Architecture and module design
└── ROADMAP.md            # Development phases and progress
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
luna_scene  → luna_core, luna_util
luna_camera → luna_util
luna_input  → luna_util, GLFW
luna3d      → all of the above
```

Key architectural decisions:
- **Double precision** for all simulation-scale coordinates (positions, velocities), downcast to float only at the GPU boundary
- **Camera-relative rendering** to eliminate floating-point jitter at Moon-scale distances (~1.7M meters)
- **Cubesphere terrain** with quadtree LOD for seamless orbital-to-surface detail
- **Reversed-Z depth buffer** (D32_SFLOAT) for precision across 0.5m–2,000km range
- **Push constants only** (no descriptor sets yet) — keeps the Vulkan surface area minimal

## License

Apache 2.0
