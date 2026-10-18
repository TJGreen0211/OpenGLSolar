#ifndef BODIES_H
#define BODIES_H

#include "Common.h"

// Number of planets (0 Mercury .. 10 Eris) and the fixed moon-table size.
// Central here so sim and the render modules (the loading-step budget, etc.)
// agree on the counts.
#define PLANET_COUNT 11
#define MAX_MOONS 21

typedef struct moonParameters {
	float radius;
	float size;
	float orbit;
	int planet;
	GLuint texture;
	GLuint normal;
} moonParameters;

typedef struct planetParameters {
	float radius;
	float size;
	float orbit;
	float axialTilt;
	float day;
	int createAtmosphere;
	int createRing;
	int numMoons;
	float ringInner;
	float ringOuter;
	GLuint texture;
	GLuint normal;
	GLuint displacement;   // height map derived from `normal` (vertex displacement)
	float dispScale;       // vertical exaggeration as a fraction of planet radius
	float dispLodBase;     // log2(texelWidth/PI): mip for the vertex height fetch

} planetParameters;

typedef struct atmosphereParameters {
	float scaleFactor;
	float E;
	vec3 C_R;
} atmosphereParameters;

// The planet / moon / atmosphere data tables (owned by sim). Handed out
// mutable (the former per-planet ringTex now lives in the Rings render module,
// not the struct; the non-const return is retained rather than threading
// const through every caller).
planetParameters *Bodies_Planets(void);
const moonParameters *Bodies_Moons(void);
// Number of moons that actually loaded (texture present on disk).
int Bodies_MoonCount(void);
const atmosphereParameters *Bodies_Atmospheres(void);

// Per-body absolute orbit / spin angles (radians, reduced into [0, TAU)),
// recomputed from simDays by Bodies_Update so changing speed never
// accumulates error.
float Bodies_OrbitSpeed(int i);
float Bodies_RotationSpeed(int i);
float Bodies_MoonOrbitSpeed(int i);

// Shared black 1x1 map the moon draw binds for the parallax step (a no-op).
GLuint Bodies_MoonDisplacementTexture(void);

// World-space centre, the same formula the per-frame draw uses.
vec3 Bodies_PlanetWorldPosition(int i);
vec3 Bodies_MoonWorldPosition(int m);

// Build the data tables. `step` advances the loading screen once per texture
// decode; it is passed in so sim stays free of a loading-screen dependency.
// The three inits mirror the three [trace] "main: planetBuilder / moonBuilder /
// atmosphereBuilder" milestones in the app.
void Bodies_InitPlanets(void (*step)(void));
void Bodies_InitMoons(void (*step)(void));
void Bodies_InitAtmospheres(void);

// Recompute every body's absolute angle (planets, spins, moons) from simDays.
void Bodies_Update(double simDays);

#endif
