# OpenGLSolar

A 3D solar system rendered in C with OpenGL 4.1 (core profile). Originally developed on macOS in the early 2010s, then revived and ported to Windows (branch `win-port`).

## Status

- **Last verified working commit: `91f2a7a`** ("Parrallax occlusion map work"). Commits after it on `master` are broken (they reference GUI code that doesn't match the rest of the tree). If behavior looks off, `91f2a7a` is the reference point.
- The `win-port` branch is at `91f2a7a` plus an **uncommitted Windows port**: glad-based GL function loading, SOIL compiled from source, MSVC build script, and startup trace instrumentation.

## Features

- Sun + 11 bodies (Mercury, Venus, Earth, Mars, Ceres, Jupiter, Saturn, Uranus, Neptune, Pluto, Eris) with real relative orbital periods, rotation durations, and axial tilts (see `sim/Bodies.c`).
- Per-planet diffuse + normal map textures (all in `assets/textures/planets/`).
- Atmospheric scattering (GPU Gems 2 ch. 16): additive sky-dome in-scatter plus per-planet surface extinction (fog) in `planet.frag`, with per-body scale factor and color — see `sim/Bodies.c`, enabled for Venus/Earth/Mars/Pluto/Ceres/Jupiter/Eris). Consistent for the camera inside and outside the shell.
- Multi-moon system: 21 moons, each with its own diffuse + normal map. Data + loads live in `sim/Bodies.c` and they're drawn by `render/Moon.c` (reusing the planet mesh); 20 of the 21 have textures on disk (`[trace] moonBuilder: 20/21`), and the builder logs the one it skips.
- 6-face skybox cubemap (`assets/textures/skybox3/`).
- GPU-instanced asteroid belt (1,500 rocks) and Kuiper belt (1,000 rocks) — `asteroid.obj` / `rock.obj` (`render/Belt.c`).
- Three parabolic-flyby comets, each with its own perihelion distance (kept close to the sun), approach angle, phase, and speed so they dive in from different directions at different times. Each comet's tail is a fan of 132 billboard sprites pointing anti-sunward, and the whole body's visibility is tied to distance from the sun (bright close in, faded to invisible far out) so the eye catches the fast bright perihelion dive, not a slow distant orbit (see `render/Comet.c`).
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
- The exe **must live in the project root** — `main()` chdirs to the exe's directory, and every asset/shader path in the code is relative to the repo root (`assets/shaders/...`, `assets/textures/...`). `build.bat` guarantees this layout; if you move the exe, expect missing textures/shaders.
- `solarsystem.exe` and `glfw3.dll` in the root plus `build/` are gitignored.
- `legacy/` is a gitignored quarantine of dead/legacy files (see `legacy/README.md`).
- `tools/` = shader-compile/debug command-line tools (keep out of `build/`).

## Original macOS build (legacy)

The project originally built with `make` in `src/` (see the legacy Makefiles, now under `legacy/makefiles/`: gcc, system frameworks, SOIL + GLFW from Homebrew/X11 paths). The Makefile hardcoded a personal install path (`/Users/tjgreen/...`). That macOS `#ifdef __APPLE__` chdir in `main()` is gone from this branch — it now locates its own directory via `GetModuleFileNameA` — and the whole macOS Makefile flow is historical. Use the Windows flow above.

## Project layout

```
build.bat                  MSVC build script (emits solarsystem.exe at repo root)
run.bat                    Launcher
.gitignore                 Build outputs, OS/editor junk, legacy quarantine
todo.txt
solarsystem.exe            Build output (gitignored)
glfw3.dll                  Build output copy (gitignored)
assets/                    (was include/)
  obj/                     asteroid.obj, rock.obj (GPU-instanced asteroid/Kuiper belts;
                           rock.obj also the comet nucleus)
  shaders/                 GLSL 410: skybox, sun, planet (normal + parallax), atmosphere, belt,
                           comet (nucleus + particle tail), UI, moons, loading (runtime-loaded)
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
  Main.c/.h                App layer: main(), GLFW setup + callbacks, input, camera follow, UI targets / moon-panel /
                           sliders, HUD, and init() (orchestrates */_InitGL; the render loop calls */*_Draw)
  render/
    Common.c/.h            GL helpers: texture loading (SOIL), shader linking, ViewMatrices,
                           glSetupMVP / glSetupPlanetLighting / bindTexture, belt+comet LCG,
                           BUFFER_OFFSET, OLD_TICKS_PER_DAY
    LoadShader.c/.h        Shader file loading + compilation
    LoadingScreen.c/.h     Full-window loading screen (progress bar over the load steps)
    Skybox.c/.h            Cubemap load / init / draw
    Sun.c/.h               Sun program + quad; opaque disc + additive glow
    Planet.c/.h            Planet program + shared mesh/VAO; per-planet draw (writes transforms)
    Orbits.c/.h            Orbit rings
    Rings.c/.h             Ring-texture synth + annulus draw
    Moon.c/.h              Moons (reuse the planet program + mesh)
    Atmosphere.c/.h        Per-planet additive sky-dome
    Belt.c/.h              GPU-instanced asteroid + Kuiper belts
    Comet.c/.h             Comet flybys (nucleus + particle tail)
  geometry/
    LinearAlg.c/.h         vec3/vec4/matrix math
    Geometry.c/.h          Shared geometry helpers
    Sphere.c/.h            Subdivided-tetrahedron mesh generator
    Obj.c/.h               Hand-rolled OBJ loader (fgetc/fscanf parsing)
  ui/
    GUI.c/.h               Textured-quad button widget system
    Camera.c/.h            View matrix, orbit/move/zoom handling
  sim/
    SimTime.c/.h           Sim clock, speed presets, pause, calendar date math
    Bodies.c/.h            Planet/moon data, texture loading, orbital motion (Bodies_* accessors)
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
4. **Dead/experimental code:** the manual experiment/backup shaders (now quarantined in `legacy/shaders-experimental/`, incl. the old "` 2`"/" copy" variants), and the old GUI-button experiment (`initializePlanetButtons()`/`drawPlanetButtons()`) is gone; the live UI is the `uiInit()` / `uiAddButton()` / `uiAddButtonTex()` widget system in `ui/GUI.c` (body buttons, per-planet moon chevrons, pause, sliders).
5. **Redundant idiom in `sim/Bodies.c`:** `*planetTexArray = *loadPlanetTextures(step, planetTexArray)` is a self-assignment no-op (the function fills the array in place and returns it). Harmless, but confusing.
6. **Compiler warnings are expected.** The MSVC build emits ~150 warnings (W3) from the legacy code (implicit double→float conversions, signed/unsigned mismatches, `GLADloadfunc` indirection mismatch at `gladLoadGL`). None affect the known-good behavior; don't chase them all at once.
7. **Shared state is now module-private.** Per-scene programs/VAOs and the planet/moon/belt/comet data are private to their module behind `*_()` accessors. What's still shared: the App's input/follow/frame state in `Main.c` (`keys`, `actionPress`, `followTarget`, `focusDolly`, `gFrame`, `deltaTime`, `lastFrame`, `appView`, `p`/`v`, `zNear`/`zFar`, `ASPECT`) and the camera externs in `Camera.c`.

## Git reference

- `master` — 22 commits total: 14 good commits up to `91f2a7a`, then 8 more (moons, planet rings, UI work, quaternion camera at tip `2760dbd`) that are **broken** — they reference GUI/UI code that doesn't match the tree. The moon/rings features exist only on master, not in the working tree.
- `win-port` — 14 commits, currently checked out. Tip is `91f2a7a` plus the uncommitted Windows port (this is the live working tree).
- The " 2"/" copy" shader files (e.g. atmosphere variants, `planet copy.*`) were manual experiment backups, not versioned state; they are quarantined in `legacy/shaders-experimental/`.
