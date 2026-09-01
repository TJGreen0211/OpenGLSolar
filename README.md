# OpenGLSolar

A 3D solar system rendered in C with OpenGL 4.1 (core profile). Originally developed on macOS in the early 2010s, then revived and ported to Windows (branch `win-port`).

## Status

- **Last verified working commit: `91f2a7a`** ("Parrallax occlusion map work"). Commits after it on `master` are broken (they reference GUI code that doesn't match the rest of the tree). If behavior looks off, `91f2a7a` is the reference point.
- The `win-port` branch is at `91f2a7a` plus an **uncommitted Windows port**: glad-based GL function loading, SOIL compiled from source, MSVC build script, and startup trace instrumentation.

## Features

- Sun + 11 bodies (Mercury, Venus, Earth, Mars, Ceres, Jupiter, Saturn, Uranus, Neptune, Pluto, Eris) with real relative orbital periods, rotation durations, and axial tilts (see `planetBuilder()` in `src/Main.c`).
- Per-planet diffuse + normal map textures (all in `assets/textures/planets/`).
- Atmospheric scattering shaders (Fresnel limb glow with per-body scale factor and color — see `atmosphereBuilder()`, enabled for Venus/Earth/Mars/Pluto/Ceres/Jupiter/Eris).
- A moon-like body orbiting Earth — note: in this commit it is rendered with Earth's own texture (`drawMoon()` binds `planetInstanceArray[2]`, `src/Main.c:687`); the dedicated `moonTexture`/`moonBuilder()` code is present but never called. Real moon systems only exist on `master`.
- 6-face skybox cubemap (`assets/textures/skybox3/`).
- Torus mesh (`assets/obj/torus.obj`) rendered near the sun.
- GPU-instanced asteroid belt (1,500 rocks) and Kuiper belt (1,000 rocks) — `asteroid.obj` / `rock.obj` (`initBelts`/`drawBelts`).
- Three parabolic-flyby comets, each with its own perihelion distance (kept close to the sun), approach angle, phase, and speed so they dive in from different directions at different times. Each comet's tail is a fan of 132 billboard sprites pointing anti-sunward, and the whole body's visibility is tied to distance from the sun (bright close in, faded to invisible far out) so the eye catches the fast bright perihelion dive, not a slow distant orbit (see `initComets`/`updateComet`/`drawComets`).
- All bodies share one subdivided-tetrahedron mesh (LOD 5 = 4096 triangles) with per-planet matrices.

## Controls

| Input        | Action                                   |
| ------------ | ---------------------------------------- |
| `W` `A` `S` `D` | Move camera                             |
| Left-drag    | Orbit camera                             |
| Scroll wheel | Zoom                                     |
| `R`          | Toggle planet orbit/rotation animation   |
| `Esc`        | Quit                                     |

Launches in **bordered fullscreen**: a normal decorated window created at the primary monitor's native size, then maximized via `glfwMaximizeWindow` (fills the work area — taskbar stays; title bar present, use it for minimize/move or just `Esc` to quit). The viewport is driven by `glfwGetFramebufferSize` each frame, so resizes and Windows DPI scaling are handled, and `ASPECT` for the projection matrix is taken from the actual maximized size. The legacy 1400x800 `WIDTH`/`HEIGHT` constants only act as a fallback if no monitor is detected.

## Building & running (Windows — current)

**Prerequisites**

- Windows 10/11 with a GPU supporting OpenGL 4.1+ (the context requested is 4.1 core, 4x MSAA).
- Visual Studio 2022 Build Tools (or full Studio) with the C++ compiler. `build.bat` locates `vcvars64.bat` under `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` or the same path without `(x86)`.
- Dependent libraries are already in the repo: `third_party\glfw\glfw-3.4.bin.WIN64` (prebuilt GLFW) and `third_party\glad\gladgen` (glad 4.1). SOIL is compiled from the in-tree source in `third_party\soil\src`.

**Steps**

1. `build.bat` — compiles everything with MSVC (`cl /O2 /MD`) and emits `solarsystem.exe` + `glfw3.dll` **in the project root** (object files go to `build\obj`).
2. `run.bat` — launches `solarsystem.exe`. You can also just double-click `solarsystem.exe`.

**Notes**

- Startup takes **~10–15 seconds**: ~40 textures (skybox, planets, normals, buttons) are loaded through SOIL on the main thread before the render loop starts. 11 of those loads are doomed — see caveat #1. `[trace] ...` lines on stderr (unbuffered) track progress through each initialization step.
- The exe **must live in the project root** — `main()` chdirs to the exe's directory, and every asset/shader path in the code is relative to the repo root (`src/shaders/...`, `assets/textures/...`). `build.bat` guarantees this layout; if you move the exe, expect missing textures/shaders.
- `solarsystem.exe` and `glfw3.dll` in the root plus `build/` are gitignored.
- `legacy/` is a gitignored quarantine of dead/legacy files (see `legacy/README.md`).
- `tools/` = shader-compile/debug command-line tools (keep out of `build/`).

## Original macOS build (legacy)

The project originally built with `make` in `src/` (see the legacy Makefiles, now under `legacy/makefiles/`: gcc, system frameworks, SOIL + GLFW from Homebrew/X11 paths). The Makefile hardcodes a personal install path (`/Users/tjgreen/...`), and `main()` contains a matching `#ifdef __APPLE__` chdir. This is historical — use the Windows flow above.

## Project layout

```
build.bat                  MSVC build script (emits solarsystem.exe at repo root)
run.bat                    Launcher
.gitignore                 Build outputs, OS/editor junk, legacy quarantine
todo.txt
solarsystem.exe            Build output (gitignored)
glfw3.dll                  Build output copy (gitignored)
assets/                    (was include/)
  obj/                     torus.obj, asteroid.obj, rock.obj (GPU-instanced asteroid/Kuiper
                           belts; rock.obj also the comet nucleus)
  textures/
    buttons/               Body icons (scripts/make_icons.py output)
    moons/
    planets/               Planet diffuse/normal maps (lowercase dir; code paths match)
    skybox3/               The 6-face cubemap actually loaded
    buttonUp.png  buttonDown.png
third_party/
  glfw/                    GLFW 3.4 WIN64 binary distribution
  glad/                    glad generator output (gl.h + gl.c, OpenGL 4.1)
  soil/                    SOIL source + headers, compiled directly on Windows (was include/SOIL/)
src/
  Main.c/.h                Entry point, scene setup, planet/atmosphere params, render loop
                           (monolith until the later split)
  shaders/                 GLSL 410: skybox, sun, planet (normal + parallax), atmosphere, belt,
                           comet (nucleus + particle tail), UI (runtime-loaded, stays in src/)
  render/
    Common.c/.h            Texture loading (SOIL), shader program linking
    LoadShader.c/.h        Shader file loading + compilation
  geometry/
    LinearAlg.c/.h         vec3/vec4/matrix math
    Geometry.c/.h          Shared geometry helpers
    Sphere.c/.h            Subdivided-tetrahedron mesh generator
    Obj.c/.h               Hand-rolled OBJ loader (fgetc/fscanf parsing)
  ui/
    GUI.c/.h               Textured-quad button widget system
    Camera.c/.h            View matrix, orbit/move/zoom handling
  sim/                     Future placeholder: SimTime, Bodies (populated when Main.c splits)
tools/                     Shader-compile/debug command-line tools (glslcheck, shadercheck,
                           sunrender, mcheck — built ad hoc); exes emit to repo root
scripts/
  make_icons.py            One-off: generates circular body button icons into assets/textures/buttons/
build/obj/                 Compile objects only (rest of build/ is drained)
legacy/                    Gitignored quarantine of dead/legacy files (see legacy/README.md)
```

## Caveats (read before modifying)

1. **Missing displacement maps.** Commit `91f2a7a` wired up parallax/height displacement, referencing `assets/textures/planets/earthDisplacement.png` and `jupiterDisplacement.png` — **neither file exists in the repo**. `loadTexture()` (`src/render/Common.c:44`) doesn't check for SOIL failure, so the app survives but the parallax feature is inert. To activate it: drop those two files in place.
2. **No error checking anywhere near legacy-code levels.** Missing files, shader compile failures, and GL errors are mostly printed or silently ignored. Don't expect robustness; don't "fix" the style wholesale unless asked.
3. **Legacy junk has been untracked** (staged as deletions, still present on disk): `src/cube` (old macOS binary), `lib/` (legacy macOS libs/headers), `include/freetype-2.4.0/` (unused vendored source), 14 `.DS_Store` files, and SOIL sample/IDE cruft under `include/SOIL/`. `.gitignore` now covers all of it. Nothing the current build uses was touched.
4. **Dead/experimental code:** the manual experiment/backup shaders (now quarantined in `legacy/shaders-experimental/`, incl. the old "` 2`"/" copy" variants), and the GUI button system — `initializePlanetButtons()` runs at startup, but `drawPlanetButtons()`/`drawButton(button1)` are **commented out in the render loop** (`src/Main.c`). Only `button1` is hit-tested on click (`checkButtonPress`), and it toggles `stopRotation` like `R` does.
5. **Redundant idiom in `planetBuilder()`:** `*planetTexArray = *loadPlanetTextures(planetTexArray)` is a self-assignment no-op (the function fills the array in place and returns it). Harmless, but confusing.
6. **The moon wears Earth's face.** `drawMoon()` (`src/Main.c:687`) binds `planetInstanceArray[2]` (Earth) for both texture and normal map; `moonBuilder()`/`moonTexture` are dead code. If you want the actual moon, add `moonBuilder()` to `main()` and point `drawMoon()` at it (or port the moon work from `master`, commits `94f57cb`/`b841e62` — which is also where the planet rings live).
7. **Compiler warnings are expected.** The MSVC build emits ~150 warnings (W3) from the legacy code (implicit double→float conversions, signed/unsigned mismatches, `GLADloadfunc` indirection mismatch at `gladLoadGL`). None affect the known-good behavior; don't chase them all at once.
8. **Global state is pervasive** (camera position, planet arrays, texture IDs, `stopRotation`, `deltaTime`, ...). Assume any function in `Main.c` can touch shared globals.

## Git reference

- `master` — 22 commits total: 14 good commits up to `91f2a7a`, then 8 more (moons, planet rings, UI work, quaternion camera at tip `2760dbd`) that are **broken** — they reference GUI/UI code that doesn't match the tree. The moon/rings features exist only on master, not in the working tree.
- `win-port` — 14 commits, currently checked out. Tip is `91f2a7a` plus the uncommitted Windows port (this is the live working tree).
- The " 2"/" copy" shader files (e.g. atmosphere variants, `planet copy.*`) were manual experiment backups, not versioned state; they are quarantined in `legacy/shaders-experimental/`.
