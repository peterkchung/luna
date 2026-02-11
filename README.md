# Luna

An open-source, first-person lunar landing simulator built in C++20/Vulkan with real NASA terrain data, accurate celestial positioning, and comprehensive telemetry.


## Objective

**What would it feel like to descend to the moon from lunar orbit?**

Through the cockpit window, the Moon's surface fills the lower half of your view — real topography from NASA's LOLA instrument, craters and ridges casting sharp shadows in the airless vacuum. Earth hangs in the black sky, positioned where it actually is relative to the Moon. The Sun casts light from the correct direction. Your instrument panel shows altitude, velocity, attitude, fuel, trajectory prediction, and range to the landing pad.

You begin your descent.


## Project Structure

```
luna/
├── src/
│   ├── main.cpp          # Entry point
│   ├── core/             # Vulkan infrastructure (RAII wrappers)
│   ├── scene/            # Renderable objects (terrain, skybox, cockpit)
│   ├── sim/              # Simulation engine (no Vulkan dependency)
│   ├── input/            # GLFW input handling and flight controls
│   ├── camera/           # Cockpit, chase, and free-look cameras
│   ├── hud/              # Telemetry instruments and overlays
│   └── util/             # Math helpers, logging, config, file I/O
├── shaders/              # GLSL vertex/fragment shaders
├── assets/               # Terrain heightmaps, SPICE kernels, OBJ models
├── external/             # Third-party dependencies (git submodules)
└── tools/                # Python scripts for NASA data retrieval
```

See [DESIGN.md](DESIGN.md) for detailed module responsibilities, rendering architecture, and design decisions.


## Development Setup

### Prerequisites

- C++20 compiler (GCC 12+, Clang 14+, MSVC 2022+)
- CMake 3.20+
- Vulkan SDK (1.3+)
- Python 3.8+ (for asset download scripts)

### Build

```bash
git clone https://github.com/<user>/luna.git
cd luna
git submodule update --init --recursive

# Download NASA data
python tools/fetch_terrain.py
python tools/fetch_kernels.py

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Run

```bash
./build/luna3d
```


## License

Apache 2.0
