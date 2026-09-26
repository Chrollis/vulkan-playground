# Vulkan-Playground

A small GPU PBR ray tracer built with Win32 and Vulkan 1.3.

## Features

- Vulkan-Hpp with `vk::raii`
- Vulkan 1.3 dynamic rendering and synchronization2
- glTF 2.0 PBR material loading
- BVH acceleration for triangle meshes
- HDR environment map IBL
- Progressive temporal accumulation
- Orbit camera
- Multiple lights and ray-traced shadows

## Requirements

- Windows 10/11
- Visual Studio 2022 or newer
- CMake 3.21+
- Vulkan SDK

## Build

```bat
cmake -S . -B build
cmake --build build --config Release
```

## Run

```bat
build\Release\Vulkan-Playground.exe
```

## Assets

The `assets/` directory is ignored by Git.

Create it locally and provide:

```text
assets/model.glb   glTF 2.0 model
assets/env.hdr     equirectangular HDR environment map
```

Optional fallback:

```text
assets/model.obj   used when model.glb is missing
```

## Controls

```text
Left mouse drag   orbit camera
Right mouse drag  pan camera
Mouse wheel       zoom
W / S             move forward / backward
A / D             move left / right
Space             move up
Shift             move down
Ctrl              move faster
Q / E             reserved for free-look (future)
, / .             rotate environment
; / '             environment intensity
[ / ]             exposure
L                 toggle light gizmos
H                 toggle HUD
R                 reset camera
Esc               exit
```