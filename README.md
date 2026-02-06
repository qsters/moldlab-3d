# MoldLab 3D

A real-time 3D emergent-behavior simulation built in **C++17** and **OpenGL 4.3+**. The simulation runs on the GPU via **compute shaders**, and the scene is visualized with a **ray-marched renderer** accelerated by a coarse **Jump Flood Algorithm (JFA)** distance field.


This is a 3D re-imagnining of my game: [MoldLab](https://store.steampowered.com/app/2454710/MoldLab/).

![BurstingSporesGif](gifs/BurstingSpores.gif)

> Platform note: **Linux + Windows** supported. **macOS not supported** (OpenGL 4.3 compute shaders).

## Technical Highlights
- **Ray-Marched Rendering**: High-quality rendering of the slime-mold behavior using ray-marching on the GPU. The CPU primarily updates the settings struct and dispatches the compute workloads. Works with both tranparent and opaque rendering:
![TransparencyDisplay](gifs/TransparencyDisplay.gif)
- **Custom Game Engine**: Fully implemented engine with core features like input handling, shader management, and rendering pipeline.
- **Optimized SDF Generation**: I implemented a novel way to optimize ray marching in 3d by using the Jump Flood Algorithm for low detailed SDF Generation. Which reduces the expensive local sampling in the marching.
- **Dynamic Simulation Parameters**: Real-time adjustment of variables such as spore count, grid size, and spore behavior via an intuitive ImGui-based interface.
![DynamicMovement](gifs/DynamicMovement.gif)
- **Shader “header injection”:** simulation structs are shared between C++ and GLSL by injecting definitions (e.g. `SimulationData`, `Spore`) into shaders at compile time.

## Dependencies
- OpenGL **4.3+** (compute shaders, `#version 430`)
- C++17 compiler
- CMake **3.15+**
- GPU drivers exposing OpenGL 4.3+


## Build Instructions

### Linux
```bash
git clone https://github.com/qsters/moldlab-3d.git
cd moldlab-3d

cmake -S . -B build
cmake --build build
./build/MoldLab3D
```

### Windows
```powershell
git clone https://github.com/qsters/moldlab-3d.git
cd moldlab-3d

cmake -S . -B build
cmake --build build
.\build\Debug\MoldLab3D.exe
```

Notes:
- This project uses CMake to automatically fetch/build **GLFW** during configuration.
- You still need working GPU drivers that expose OpenGL 4.3+.
- On Linux, the app forces the GLFW X11 backend to avoid Wayland loader issues. Ensure X11 or XWayland is installed.

## Usage
- **Camera Controls**:
    - Arrow keys: Move the camera around the grid.
    - `A` / `D`: Zoom in and out.
- **Adjust Simulation Settings**:
    - Use sliders in the ImGui interface to modify parameters like:
        - Spore Count
        - Grid Size
        - Spore Speed, Decay, and Turn Speed
        - Sensor Distance and Angle
- **Reset and Randomize**:
    - Buttons in the UI allow for resetting spores and clearing the grid.
- **Rendering Options**:
    - Toggle transparency and grid wrapping using UI checkboxes.

## Core Files
### 1. Game Engine
- `GameEngine.h` & `GameEngine.cpp`: Core game engine implementation, including window management, input handling, and rendering pipeline.

### 2. Simulation Logic
- `MoldLabGame.h` & `MoldLabGame.cpp`: Contains simulation-specific logic, including shader initialization, compute shader dispatch, and user interaction.

### 3. Important Shaders
- `renderer.glsl` is the shader used for rendering to the screen.
- `move_spores.glsl` defines the behavior for moving the spores in the environment.
- `jump_flood_init.glsl` is the init shader for the Jump Flood Algorithm
- `jump_flood_step.glsl` steps through the Jump Flood Algorithm.


## LicenseNotes:
- This project uses CMake to automatically fetch/build **GLFW** during configuration.
- You still need working GPU drivers that expose OpenGL 4.3+.
- On Linux, the app forces the GLFW X11 backend to avoid Wayland loader issues. Ensure X11 or XWayland is installed.
