# sim

Simulation core, pulled out of the old `src/Main.c` monolith:

- `SimTime.c/h` — sim clock, speed presets, pause, and calendar date math (Julian date <-> y/M/d H:m:s).
- `Bodies.c/h` — the planet/moon data table, diffuse/normal texture loading, and per-frame orbital motion. The `planets`/`moons`/`atmospheres` tables are module-private, exposed through the `Bodies_*` accessors (motion values, world positions, the moon-displacement texture, and the `PLANET_COUNT`/`MAX_MOONS` limits).
