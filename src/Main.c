#include <stdio.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include "Main.h"

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
	GLuint displacement;
	GLuint ringTex;
	mat4 planetLocation;

} planetParameters;

typedef struct atmosphereParameters {
	float scaleFactor;
	float E;
	vec3 C_R;
} atmosphereParameters;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void window_size_callback(GLFWwindow* window, int width, int height);
GLuint loadShader(char *vertex_path, char *fragement_path);
void initLoadingScreen();
void renderLoadingScreen(float progress);
void finishLoadingStep();
#ifdef _WIN32
static GLADapiproc gladGetProcAddress(const char *name);
#endif

const float WIDTH = 1400, HEIGHT = 800;
float ASPECT = 1400.0f/800.0f;

sphere planet, sun, atmosphere;
obj object;
int keys;
int actionPress;

float zNear = 0.5, zFar = 100000.0;
float theta = 0.0;

GLuint sunTexture;
GLuint sunNormal;

GLuint moonNormalTexture;
GLuint moonDisplacementTexture;
GLuint skyboxShader, sunShader, planetShader, atmosphereShader, ringShader;
GLuint vertUIShader, fragUIShader;
GLuint vPosition, vNormal, vTangent, vBitangent;
GLuint planetVAO, planetVBO, skyboxVAO, skyboxVBO, objectVAO, objectVBO, atmosphereVAO, atmosphereVBO;
GLuint sunQuadVAO, sunQuadVBO;
GLuint orbitShader, orbitVAO, orbitVBO;
GLuint ringVAO[11], ringVBO[11];
ring ringArray[11];

// GPU-instanced asteroid + Kuiper belts (initBelts/drawBelts). Each rock
// carries its whole orbit as instance data; the vertex shader advances it
// from uTime, so the belts cost two instanced draws per frame.
typedef struct beltRockData {
	float orbitRadius, height, angSpeed, phase;  // aOrbit
	float scale, spinSpeed, spinPhase, tilt;     // aRock
} beltRockData;

typedef struct belt {
	char *meshPath;
	obj mesh;
	GLuint vao, vbo, ibo;
	int count;
} belt;

belt asteroidBelt;
belt kuiperBelt;
GLuint beltShader;
float beltTime = 0.0f;

// Highly-eccentric-ellipse comet with a billboarded particle tail (initComets /
// updateComet / drawComets). The nucleus is a small rock; the tail is a fan
// of camera-facing soft sprites run anti-sunward from the nucleus, with
// length and brightness both proportional to 1/r^2.
// Number of comets in the scene. Each gets its own perihelion distance,
// aphelion distance, approach angle, orbit phase, and speed (seeded in
// initComets), so they cross the system at different intervals from
// different directions.
#define COMET_COUNT        3
// Each comet is a flat highly-eccentric ellipse (sun at the focus): a close
// perihelion q (dive into the sun) and a far aphelion (long arc out into the
// system). Its visibility is tied to distance from the sun exactly like a
// real comet: fully bright close in, smoothly fading to invisible beyond
// COMET_VIS_FAR * q. The full anomaly wrap at +/-PI happens at aphelion while
// faded out, so the slow far arc is never seen and what the eye catches is
// the fast, bright close pass. Keeping this tight (~2.6q) means there is no
// "faint dot drifting in the distance" at all.
#define COMET_VIS_FAR      2.6f      // invisible beyond this many perihelion distances
#define COMET_TAIL_MAX     12000.0f  // tail length at perihelion
#define COMET_ORBIT_K      70000.0f  // base speed; dTheta/frame = K / r^2, scaled per comet
#define COMET_PARTICLES    132
#define COMET_NUCLEUS_SCALE 14.0f

typedef struct comet {
	obj mesh;
	GLuint vao, vbo;          // nucleus: positions + normals
	GLuint tailVao, quadVbo, tailVbo;
	int  particleCount;
	float theta;              // true anomaly (rad from perihelion), decreasing
	float q;                  // perihelion distance
	float e;                  // eccentricity = (rA - q) / (rA + q)
	float p;                  // semi-latus rectum = q (1 + e)
	float dir;                // orientation: world angle of perihelion (rad)
	float orbitK;             // per-comet speed (dTheta per 60-Hz reference frame = orbitK / r^2)
	float spin;
	// Per-frame derived state (set by updateComet, read by drawComets).
	vec3 pos;
	float r;
	vec3 tailDir;
	float tailLen, tailWidth, tailAlpha;
	float vis;                  // 0..1 distance-based visibility (bright in, fades out)
} comet;

comet comets[COMET_COUNT];
GLuint cometShader, tailShader;

// Defined further down; drawn before initMVP's definition, so forward-declare.
void initMVP(int shader, mat4 m, mat4 v);

void initComets();
void updateComet(comet *c, float timeScale);
void drawComets();
GLuint shadowFBO, depthMap, rboDepth;
GLuint ModelView, projection, model, view;
mat4 p, v;

GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;
int stopRotation = 0;
int gFrame = 0;   // bumped once per render-loop iteration; keys the initMVP cache

/* Camera follow state, set by the UI buttons. The value is the same space as
   a uiHitTest result (and a uiButton id), so each trackable body has one:
   UI_ACT_NONE (-3) = free camera (WASD moves it), UI_ACT_SUN (-1) = sun/
   overview pinned at the origin, 0..10 = planetInstanceArray[i], 100+m =
   moonInstanceArray[m]. Pause (-2) and toggles (200+) are controls, never
   targets. focusDolly runs the one-shot distance glide right after a target
   button is clicked. */
int followTarget = UI_ACT_NONE;
int focusDolly = 0;

// Loading screen state. loadWindow is the GL context the loading frames are
// drawn on; it is set in main() right after setupGLFW() and read by the
// loader functions (planet/moon texture loops) so they can redraw the bar.
GLFWwindow *loadWindow = NULL;
GLuint loadingShader, loadingVAO, loadingVBO;
int loadStepDone = 0;


// Worst-case number of discrete load steps: skybox (1) + sun (2) +
// planet textures (11x3) + moons (neutral normal 1 + displacement 1 + up to
// MAX_MOONS textures) + atmosphere (1) + init (1) + UI buttons (13) +
// moon panels (up to 11 toggles + MAX_MOONS moon buttons) + UI shader (1).
// If some moon textures are missing the bar tops out below 100% and the
// final frame forces it to 1.0, which is fine.
#define LOAD_TOTAL_STEPS (1 + 2 + 33 + 2 + MAX_MOONS + 16 + 11 + MAX_MOONS)

/* One entry per button in the right-edge column, top to bottom: sun/
   overview, planets in planetInstanceArray order (0..10), pause last.
   actId is the uiHitTest value (-2 pause, -1 sun, 0..10 planet). Moons are
   NOT in this list — setupUIMoonButtons() adds each planet's chevron toggle
   (200+planet) and moon buttons (100+moon) dynamically. */
struct uiTargetDef {
	const char *icon;
	const char *activeIcon; // alternate art for the active/pressed state
	int actId;
};

#define UI_TARGET_COUNT 13
static const struct uiTargetDef uiTargets[UI_TARGET_COUNT] = {
	{ "include/textures/buttons/sun.png",     NULL, UI_ACT_SUN },
	{ "include/textures/buttons/mercury.png", NULL, 0 },
	{ "include/textures/buttons/venus.png",   NULL, 1 },
	{ "include/textures/buttons/earth.png",   NULL, 2 },
	{ "include/textures/buttons/mars.png",    NULL, 3 },
	{ "include/textures/buttons/ceres.png",   NULL, 4 },
	{ "include/textures/buttons/jupiter.png", NULL, 5 },
	{ "include/textures/buttons/saturn.png",  NULL, 6 },
	{ "include/textures/buttons/uranus.png",  NULL, 7 },
	{ "include/textures/buttons/neptune.png", NULL, 8 },
	{ "include/textures/buttons/pluto.png",   NULL, 9 },
	{ "include/textures/buttons/eris.png",    NULL, 10 },
	{ "include/textures/buttonUp.png",        "include/textures/buttonDown.png", UI_ACT_PAUSE },
};

#define MAX_MOONS 21
// Vertices per orbit circle (drawOrbits draws a GL_LINE_LOOP of this size).
#define ORBIT_SEGMENTS 256
int numPlanets = 11;
planetParameters planetInstanceArray[11];
atmosphereParameters atmosphereArray[11];
moonParameters moonInstanceArray[MAX_MOONS];
mat4 modelMatrices[11];
mat4 planetTransform[11];
float orbitSpeedArray[11] = {0};
float rotationSpeedArray[11] = {0};
float moonOrbitSpeedArray[MAX_MOONS] = {0};
int loadedMoonCount = 0;
float planetScaleMult = 100.0;
float planetRadMult = 2000.0;
float moonRadMult = 500.0;
// World-space radius of the sun's billboard (its on-screen size is this value
// projected at the sun's distance, so the sun shrinks as the camera pulls
// back). Tune larger for a bigger sun, smaller for a smaller one.
const float SUN_GLOW_RADIUS = 3000.0f;
vec3 translation;

float getScreenWidth()
{
	return HEIGHT;
}

float getScreenHeight()
{
	return WIDTH;
}

int *loadPlanetTextures(int planetTexArray[])
{
	char *textureStrings[] = {"include/textures/Planets/mercury.jpg",
		"include/textures/Planets/venus.png",
		"include/textures/Planets/earth.jpg",
		"include/textures/Planets/mars.jpg",
		"include/textures/Planets/ceres.jpg",
		"include/textures/Planets/jupiter.jpg",
		"include/textures/Planets/saturn.png",
		"include/textures/Planets/uranus.jpg",
		"include/textures/Planets/neptune.jpg",
		"include/textures/Planets/pluto.png",
		"include/textures/Planets/eris.jpg"};
	int arraySize = sizeof(textureStrings)/sizeof(textureStrings[0]);
	for(int i = 0; i < arraySize; i++)
	{
		planetTexArray[i] = loadTexture(textureStrings[i]);
		finishLoadingStep();
	}
	return planetTexArray;
}

int *loadPlanetNormals(int planetTexArray[])
{
	char *textureStrings[] = {"include/textures/Planets/mercuryNormal.png",
		"include/textures/Planets/venusNormal.png",
		"include/textures/Planets/earthNormal.jpg",
		"include/textures/Planets/marsNormal.png",
		"include/textures/Planets/ceresNormal.png",
		"include/textures/Planets/jupiterNormal.png",
		"include/textures/Planets/saturnNormal.png",
		"include/textures/Planets/uranusNormal.png",
		"include/textures/Planets/neptuneNormal.png",
		"include/textures/Planets/plutoNormal.png",
		"include/textures/Planets/erisNormal.png"};
	int arraySize = sizeof(textureStrings)/sizeof(textureStrings[0]);
	for(int i = 0; i < arraySize; i++)
	{
		planetTexArray[i] = loadTexture(textureStrings[i]);
		finishLoadingStep();
	}
	return planetTexArray;
}

int *loadPlanetDisplacementMap(int planetTexArray[])
{
	char *textureStrings[] = {"include/textures/Planets/earthDisplacement.png",
		"include/textures/Planets/earthDisplacement.png",
		"include/textures/Planets/earthDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png"};
	int arraySize = sizeof(textureStrings)/sizeof(textureStrings[0]);
	for(int i = 0; i < arraySize; i++)
	{
		planetTexArray[i] = loadTexture(textureStrings[i]);
		finishLoadingStep();
	}
	return planetTexArray;
}

void planetBuilder()
{
	planetParameters mercury, venus, earth, mars, eris, jupiter, saturn, uranus, neptune, pluto, ceres;
	int numTextures = 11;
	int planetTexArray[11];
	*planetTexArray = *loadPlanetTextures(planetTexArray);

	int planetNormArray[11];
	*planetNormArray = *loadPlanetNormals(planetNormArray);

	int planetDisplacementArray[11];
	*planetDisplacementArray = *loadPlanetDisplacementMap(planetDisplacementArray);

	mercury.radius = 0.4f*planetRadMult; mercury.size = 0.38f*planetScaleMult; mercury.orbit = 0.240846f; mercury.axialTilt = 0.034f; mercury.day = 58.646f; mercury.createAtmosphere = 0; mercury.createRing = 0;
	mercury.texture = planetTexArray[0]; mercury.normal = planetNormArray[0]; mercury.displacement = planetDisplacementArray[0];
	venus.radius = 0.7f*planetRadMult; venus.size = 0.9499f*planetScaleMult; venus.orbit = 0.615198f; venus.axialTilt = 177.36f; venus.day = 243.025f; venus.createAtmosphere = 1; venus.createRing = 0;
	venus.texture = planetTexArray[1]; venus.normal = planetNormArray[1]; venus.displacement = planetDisplacementArray[1];
	earth.radius = 1.0f*planetRadMult; earth.size = 1.0f*planetScaleMult; earth.orbit = 1.0f; earth.axialTilt = 23.4392811f; earth.day = 1; earth.createAtmosphere = 1; earth.createRing = 0;
	earth.texture = planetTexArray[2]; earth.normal = planetNormArray[2]; earth.displacement = planetDisplacementArray[2];
	mars.radius = 1.5f*planetRadMult; mars.size = 0.533f*planetScaleMult; mars.orbit = 1.88f; mars.axialTilt = 25.19f; mars.day = 1.025957f; mars.createAtmosphere = 1; mars.createRing = 0;
	mars.texture = planetTexArray[3]; mars.normal = planetNormArray[3]; mars.displacement = planetDisplacementArray[3];
	ceres.radius = 2.77f*planetRadMult; ceres.size = 0.07f*planetScaleMult; ceres.orbit = 4.6f; ceres.axialTilt = 4; ceres.day = 0.3781f; ceres.createAtmosphere = 1; ceres.createRing = 0;
	ceres.texture = planetTexArray[4]; ceres.normal = planetNormArray[4]; ceres.displacement = planetDisplacementArray[4];
	jupiter.radius = 5.2f*planetRadMult; jupiter.size = 11.209f*planetScaleMult; jupiter.orbit = 11.8618f; jupiter.axialTilt = 3.13f; jupiter.day = 0.413f; jupiter.createAtmosphere = 1; jupiter.createRing = 0;
	jupiter.texture = planetTexArray[5]; jupiter.normal = planetNormArray[5]; jupiter.displacement = planetDisplacementArray[5];
	// ringInner/ringOuter are the ring edges as a multiple of the planet radius
	saturn.radius = 9.5f*planetRadMult; saturn.size = 9.45f*planetScaleMult; saturn.orbit = 29.4571f; saturn.axialTilt = 26.73f; saturn.day = 0.439583f; saturn.createAtmosphere = 0; saturn.createRing = 1; saturn.ringInner = 1.24f; saturn.ringOuter = 2.27f;
	saturn.texture = planetTexArray[6]; saturn.normal = planetNormArray[6]; saturn.displacement = planetDisplacementArray[6];
	uranus.radius = 19.2f*planetRadMult; uranus.size = 4.007f*planetScaleMult; uranus.orbit = 84.0205f; uranus.axialTilt = 97.77f; uranus.day = 0.71833f; uranus.createAtmosphere = 0; uranus.createRing = 1; uranus.ringInner = 1.55f; uranus.ringOuter = 2.05f;
	uranus.texture = planetTexArray[7]; uranus.normal = planetNormArray[7]; uranus.displacement = planetDisplacementArray[7];
	neptune.radius = 30.1f*planetRadMult; neptune.size = 3.883f*planetScaleMult; neptune.orbit = 164.8f; neptune.axialTilt = 28.32f; neptune.day = 0.6713f; neptune.createAtmosphere = 0; neptune.createRing = 0;
	neptune.texture = planetTexArray[8]; neptune.normal = planetNormArray[8]; neptune.displacement = planetDisplacementArray[8];
	pluto.radius = 39*planetRadMult; pluto.size = 0.18f*planetScaleMult; pluto.orbit = 248; pluto.axialTilt = 119.591f; pluto.day = 6.387230f; pluto.createAtmosphere = 1; pluto.createRing = 0;
	pluto.texture = planetTexArray[9]; pluto.normal = planetNormArray[9]; pluto.displacement = planetDisplacementArray[9];
	eris.radius = 68*planetRadMult; eris.size = 0.182f*planetScaleMult; eris.orbit = 558.04f; eris.axialTilt = 61.45f; eris.day = 1.079167f ; eris.createAtmosphere = 1; eris.createRing = 0;
	eris.texture = planetTexArray[10]; eris.normal = planetNormArray[10]; eris.displacement = planetDisplacementArray[10];

	planetInstanceArray[0] = mercury;
	planetInstanceArray[1] = venus;
	planetInstanceArray[2] = earth;
	planetInstanceArray[3] = mars;
	planetInstanceArray[4] = ceres;
	planetInstanceArray[5] = jupiter;
	planetInstanceArray[6] = saturn;
	planetInstanceArray[7] = uranus;
	planetInstanceArray[8] = neptune;
	planetInstanceArray[9] = pluto;
	planetInstanceArray[10] = eris;

}

void atmosphereBuilder() {
	atmosphereParameters mercury, venus, earth, mars, eris, jupiter, saturn, uranus, neptune, pluto, ceres;

	// E   : solar brightness fed to the O'Neil in-scatter integral. Earth = 14.3
	//       is the reference; the rest scale by inverse-square solar flux
	//       (1/d_AU^2) so inner planets glow hotter and outer ones dimmer.
	// C_R : Rayleigh tint (the K_R*C_R term; the white Mie term is summed on top),
	//       so each limb's hue follows C_R's RGB ratios. Earth = sky-blue.
	// Only bodies flagged createAtmosphere draw a shell; the rest keep defaults.
	mercury.scaleFactor = 1.000f;mercury.E = 14.3f; 	mercury.C_R.x = 0.3f; 	mercury.C_R.y = 0.7f; 	mercury.C_R.z = 1.0f;
	// Dense CO2 over H2SO4 cloud decks -> warm cream; ~1.9x Earth's solar flux.
	venus.scaleFactor = 1.3008f; venus.E = 27.3f; 	venus.C_R.x = 1.0f; 	venus.C_R.y = 0.86f; 	venus.C_R.z = 0.66f;
	// N2/O2 Rayleigh blue -- the reference set (leave as-is).
	earth.scaleFactor = 1.0212f; earth.E = 14.3f; 	earth.C_R.x = 0.3f; 	earth.C_R.y = 0.7f; 	earth.C_R.z = 1.0f;
	// Thin, dusty CO2 atmosphere -> butterscotch/rose; ~0.43x Earth's solar flux.
	mars.scaleFactor = 1.0210f; 	mars.E = 6.2f; 		mars.C_R.x = 1.0f; 	mars.C_R.y = 0.60f; 	mars.C_R.z = 0.45f;
	// Dwarf planet, trace N2/CH4 ices -> faint pale ice-blue. True flux ~0.003
	// (invisible); E = 1.5 keeps the thin shell (scaleFactor 1.001) faintly visible.
	eris.scaleFactor = 1.001f; 	eris.E = 1.5f; 		eris.C_R.x = 0.80f; 	eris.C_R.y = 0.86f; 	eris.C_R.z = 0.95f;
	// H2/He gas giant, ammonia-charged cloud haze -> soft warm cream; ~1/27x sun.
	jupiter.scaleFactor = 1.1448f;jupiter.E = 0.53f; 	jupiter.C_R.x = 1.0f; 	jupiter.C_R.y = 0.93f; 	jupiter.C_R.z = 0.80f;
	saturn.scaleFactor = 0.0f; 	saturn.E = 14.3f; 	saturn.C_R.x = 0.3f; 	saturn.C_R.y = 0.7f; 	saturn.C_R.z = 1.0f;
	uranus.scaleFactor = 0.0f; 	uranus.E = 14.3f; 	uranus.C_R.x = 0.3f; 	uranus.C_R.y = 0.7f; 	uranus.C_R.z = 1.0f;
	neptune.scaleFactor = 0.0f; 	neptune.E = 14.3f; 	neptune.C_R.x = 0.3f; 	neptune.C_R.y = 0.7f; 	neptune.C_R.z = 1.0f;
	// Thin N2/CH4 haze -> pale ice-blue. True flux ~0.009 (invisible); E = 2.0
	// keeps the (stylized) shell faintly visible.
	pluto.scaleFactor = 3.5000f; pluto.E = 2.0f; 	pluto.C_R.x = 0.75f; 	pluto.C_R.y = 0.83f; 	pluto.C_R.z = 1.0f; // pale ice-blue (kept at 1.0, the blue of the N2 haze)
	// Trace water-vapor haze (near-airless) -> faint blue-white; shell suppressed
	// (scaleFactor 0), values kept for completeness.
	ceres.scaleFactor = 0.0f; 	ceres.E = 1.9f;		ceres.C_R.x = 0.85f; 	ceres.C_R.y = 0.90f; 	ceres.C_R.z = 1.0f;

	atmosphereArray[0] = mercury;
	atmosphereArray[1] = venus;
	atmosphereArray[2] = earth;
	atmosphereArray[3] = mars;
	atmosphereArray[4] = ceres;
	atmosphereArray[5] = jupiter;
	atmosphereArray[6] = saturn;
	atmosphereArray[7] = uranus;
	atmosphereArray[8] = neptune;
	atmosphereArray[9] = pluto;
	atmosphereArray[10] = eris;

}

static int fileExists(const char *path)
{
#ifdef _WIN32
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
	struct stat st;
	return stat(path, &st) == 0 && S_ISREG(st.st_mode);
#endif
}

// Moons have no per-moon normal or displacement maps on disk. A neutral
// tangent-space normal keeps shading plain (loadTexture's black 1x1 fallback
// would remap to N = (-1,-1,-1)), and a black 1x1 displacement keeps the
// planet shader's parallax step a no-op.
static GLuint createNeutralNormalTexture()
{
	unsigned char px[3] = {128, 128, 255};
	GLuint normalTex;
	glGenTextures(1, &normalTex);
	glBindTexture(GL_TEXTURE_2D, normalTex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	return normalTex;
}

void moonBuilder()
{
	struct moonDef {
		const char *name;
		const char *path;
		int planet;
		float radius;
		float size;
		float orbit;
	};

	// Historical moon data: radius * moonRadMult, size * planetScaleMult.
	// Only moons whose texture file exists are drawn, so dropping a file
	// into one of these paths re-adds that moon.
	static const struct moonDef defs[MAX_MOONS] = {
		{ "moon",      "include/textures/Planets/moon.jpg",          2,  1.0f,       0.272704f,  0.083f     },
		{ "phobos",    "include/textures/moons/phobos.jpg",     3,  0.015609f,  0.001413f,  0.063014f  },
		{ "deimos",    "include/textures/moons/deimos.jpg",     3,  0.037919f,  0.000306f,  0.003425f  },
		{ "callisto",  "include/textures/moons/callisto.jpg",   5,  4.883117f,  0.376707f,  0.046575f  },
		{ "europa",    "include/textures/moons/europa.jpg",     5,  1.742597f,  0.24329f,   0.009703f  },
		{ "ganymede",  "include/textures/moons/ganymede.jpg",   5,  2.779221f,  0.206498f,  0.292123f  },
		{ "io",        "include/textures/moons/io.jpg",         5,  0.680519f,  0.142936f,  0.004849f  },
		{ "dione",     "include/textures/moons/dione.jpg",      6,  0.98026f,   0.088134f,  0.006849f  },
		{ "enceladus", "include/textures/moons/enceladus.jpg",  6,  0.384179f,  0.024643f,  0.003753f  },
		{ "iapetus",   "include/textures/moons/iapetus.jpg",    6,  35.224156f, 0.115461f,  0.216438f  },
		{ "mimas",     "include/textures/moons/mimas.jpg",      6,  0.481919f,  0.010909f,  0.002581f  },
		{ "rhea",      "include/textures/moons/rhea.jpg",       6,  1.368935f,  0.119918f,  0.012329f  },
		{ "tethys",    "include/textures/moons/tethys.jpg",     6,  0.766234f,  0.04183f,   0.005171f  },
		{ "titan",     "include/textures/moons/titan.png",      6,  3.173688f,  0.404175f,  0.043607f  },
		{ "ariel",     "include/textures/moons/ariel.jpg",      7,  0.495844f,  0.0908f,    0.006545f  },
		{ "miranda",   "include/textures/moons/miranda.jpg",    7,  0.336078f,  0.036886f,  0.003836f  },
		{ "oberon",    "include/textures/moons/oberon.jpg",     7,  1.513247f,  0.1194f,    0.036886f  },
		{ "titania",   "include/textures/moons/titania.jpg",    7,  1.132234f,  0.1235f,    0.023836f  },
		{ "umbriel",   "include/textures/moons/umbriel.jpg",    7,  0.429151f,  0.091822f,  0.011342f  },
		{ "triton",    "include/textures/moons/triton.jpg",     8,  0.921452f,  0.2122f,   -0.016101f  },
		{ "charon",    "include/textures/moons/charon.jpg",     9,  0.045548f,  0.095f,     0.017479f  },
	};

	for(int i = 0; i < 11; i++)
		planetInstanceArray[i].numMoons = 0;

	moonNormalTexture = createNeutralNormalTexture();
	finishLoadingStep();
	moonDisplacementTexture = loadTexture("include/textures/Planets/moonDisplacement.png");
	finishLoadingStep();

	// defs are ordered by planet, so appending keeps each planet's moons
	// contiguous in moonInstanceArray (drawMoon walks it sequentially)
	for(int i = 0; i < MAX_MOONS; i++)
	{
		if(!fileExists(defs[i].path))
		{
			fprintf(stderr, "[trace] moonBuilder: skipped %s (no texture at %s)\n", defs[i].name, defs[i].path);
			continue;
		}
		moonParameters m;
		m.planet = defs[i].planet;
		m.radius = defs[i].radius * moonRadMult;
		m.size = defs[i].size * planetScaleMult;
		m.orbit = defs[i].orbit;
		m.texture = loadTexture(defs[i].path);
		finishLoadingStep();
		m.normal = moonNormalTexture;
		moonInstanceArray[loadedMoonCount] = m;
		planetInstanceArray[defs[i].planet].numMoons++;
		loadedMoonCount++;
	}
	fprintf(stderr, "[trace] moonBuilder: %d/%d moons have textures\n", loadedMoonCount, MAX_MOONS);
}

// ---------------------------------------------------------------------------
// Ring textures
//
// The old commit loaded include/textures/Planets/saturnRing.png, which was
// never committed. Synthesize an equivalent radial band profile instead: a
// 512x2 RGBA strip whose RGB is the ring colour and whose A is the
// per-radius brightness. u = 0 is the inner ring edge, u = 1 the outer one.
// ---------------------------------------------------------------------------

#define RING_TEX_WIDTH 512

static float ringGaussian(float u, float c, float w)
{
	float d = (u - c)/w;
	return expf(-0.5f*d*d);
}

// Saturn: D (faint inner), C, bright B, dark Cassini division, A, thin F
static float saturnRingLuma(float u)
{
	float l;
	if(u < 0.03f)      l = 0.10f;
	else if(u < 0.20f) l = 0.28f;
	else if(u < 0.58f) l = 0.85f;
	else if(u < 0.66f) l = 0.14f;
	else               l = 0.55f;

	if(u > 0.895f && u < 0.915f)
		l *= 0.25f;
	if(u >= 0.985f)
		l = 0.35f;

	l *= 0.93f + 0.07f*sinf(u*300.0f + 1.7f)*sinf(u*77.0f);
	return l;
}

// Uranus: very faint narrow rings, brightest = epsilon ring near u ~ 0.9
static float uranusRingLuma(float u)
{
	float l = 0.03f;
	l += 0.10f*ringGaussian(u, 0.30f, 0.015f);
	l += 0.14f*ringGaussian(u, 0.45f, 0.020f);
	l += 0.12f*ringGaussian(u, 0.62f, 0.012f);
	l += 0.55f*ringGaussian(u, 0.90f, 0.012f);
	if(l > 1.0f)
		l = 1.0f;
	return l;
}

static GLuint uploadRingTexture(float baseR, float baseG, float baseB, float (*luma)(float))
{
	unsigned char pixels[RING_TEX_WIDTH*2*4];

	for(int x = 0; x < RING_TEX_WIDTH; x++)
	{
		float l = luma((float)x/(float)(RING_TEX_WIDTH - 1));
		for(int y = 0; y < 2; y++)
		{
			unsigned char *px = &pixels[(x*2 + y)*4];
			px[0] = (unsigned char)(baseR*255.0f);
			px[1] = (unsigned char)(baseG*255.0f);
			px[2] = (unsigned char)(baseB*255.0f);
			px[3] = (unsigned char)(l*255.0f);
		}
	}

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RING_TEX_WIDTH, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	return textureID;
}

// planetInstanceArray order from planetBuilder(): 6 = saturn, 7 = uranus
static GLuint generateRingTexture(int planetIndex)
{
	if(planetIndex == 6)
		return uploadRingTexture(0.93f, 0.85f, 0.70f, saturnRingLuma);
	return uploadRingTexture(0.65f, 0.75f, 0.88f, uranusRingLuma);
}

vec3 *generateTangents(int vertexNumber, vec3 *points, vec3 *tangent)
{
	vec3 axis = {0.0, 0.0, 1.0};
	vec3 t;

	// The fragment shader samples the texture with the equirectangular
	// mapping u = atan2(y, x), v = asin(z), so the tangent is the
	// direction of increasing u: cross(axis, point) = (-y, x, 0).
	for(int i = 0; i < vertexNumber; i++)
	{
		t = crossvec3(axis, points[i]);
		if(t.x*t.x + t.y*t.y + t.z*t.z < 1e-8f)
		{
			// Exactly on a pole where the mapping degenerates.
			t.x = 1.0; t.y = 0.0; t.z = 0.0;
		}
		tangent[i] = normalizevec3(t);
	}
	return tangent;
}

vec3 *generateSmoothNormals(int vertexNumber, vec3 *vna, vec3 *pointArray, vec3 *normalArray)
{
	vec3 vn;

	for(int i = 0; i < vertexNumber; i++)
	{
		vec3 tempvn = {0.0, 0.0, 0.0};
		vn = pointArray[i];
		for(int j = 0; j < vertexNumber; j++)
		{
			if(vn.x == pointArray[j].x && vn.y == pointArray[j].y && vn.z == pointArray[j].z)
			{
				tempvn = plusequalvec3(tempvn, normalArray[j]);
			}
		}
		vna[i] = normalizevec3(tempvn);
	}
	return vna;
}

/*void *generateTexCoords()
{
	vec2 temp[vertexNumber];
	for(int i = 0; i < vertexNumber; i++)
	{
		temp[i].x = atan2(points[i].y, points[i].x) / 3.1415926 + 1.0 * 0.5;
        temp[i].y = asin(points[i].z) / 3.1415926 + 0.5;
	}
}
*/

void initObject() {
	object = ObjLoadModel("include/obj/torus.obj");

	glGenVertexArrays(1, &objectVAO);
	glBindVertexArray(objectVAO);
	glGenBuffers(1, &objectVBO);
	glBindBuffer(GL_ARRAY_BUFFER, objectVBO);
	glBufferData(GL_ARRAY_BUFFER, object.size + object.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, object.size, object.points);
	glBufferSubData(GL_ARRAY_BUFFER, object.size, object.nsize, object.normals);
	vPosition = glGetAttribLocation(sunShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));

    vNormal = glGetAttribLocation(sunShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(object.size));
	glBindVertexArray(0);
}

// Tiny LCG: belts want a randomized-but-reproducible layout, and this keeps
// the app's global rand() stream untouched.
static unsigned int beltSeed = 0x5EEDB71u;
static float beltRand()
{
	beltSeed = beltSeed * 1664525u + 1013904223u;
	return (float)(beltSeed >> 8) / (float)(1u << 24);
}

void initBelt(belt *b, char *path, int count,
	float rMin, float rMax, float maxHeight, float sMin, float sMax)
{
	b->meshPath = path;
	b->count = count;
	b->mesh = ObjLoadModel(path);

	beltRockData *data = malloc(sizeof(beltRockData) * count);
	for(int i = 0; i < count; i++)
	{
		// Orbit radius: uniform across the belt's annulus.
		data[i].orbitRadius = rMin + beltRand() * (rMax - rMin);
		// Small random height above/below the ecliptic.
		data[i].height = (beltRand() * 2.0f - 1.0f) * maxHeight;
		// Same angular-speed rule the planets use, 0.001f/period with
		// period == radius/AU here, i.e. 0.002f/radiusUnits per tick,
		// with +/-15% jitter so the belt doesn't rotate as one solid ring.
		data[i].angSpeed = 0.002f / data[i].orbitRadius * (0.85f + 0.30f * beltRand());
		data[i].phase = beltRand() * 6.2831853f;
		data[i].scale = sMin + beltRand() * (sMax - sMin);
		data[i].spinSpeed = (0.5f + beltRand()) * 0.01f;
		if(beltRand() < 0.5f)
			data[i].spinSpeed = -data[i].spinSpeed;
		data[i].spinPhase = beltRand() * 6.2831853f;
		data[i].tilt = (beltRand() * 2.0f - 1.0f) * 0.5f;
	}

	glGenVertexArrays(1, &b->vao);
	glBindVertexArray(b->vao);

	// Mesh stream: positions then normals (same layout as initObject).
	glGenBuffers(1, &b->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
	glBufferData(GL_ARRAY_BUFFER, b->mesh.size + b->mesh.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, b->mesh.size, b->mesh.points);
	glBufferSubData(GL_ARRAY_BUFFER, b->mesh.size, b->mesh.nsize, b->mesh.normals);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(b->mesh.size));

	// Instance stream: one beltRockData per rock, advanced per-instance.
	glGenBuffers(1, &b->ibo);
	glBindBuffer(GL_ARRAY_BUFFER, b->ibo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(beltRockData) * count, data, GL_STATIC_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(beltRockData), BUFFER_OFFSET(0));
	glVertexAttribDivisor(2, 1);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(beltRockData), BUFFER_OFFSET(4*sizeof(GLfloat)));
	glVertexAttribDivisor(3, 1);

	glBindVertexArray(0);
	free(data);
}

void initBelts()
{
	createShader(&beltShader, "src/shaders/belt.vert",
		"src/shaders/belt.frag");

	beltSeed = 0x5EEDB71u;

	// Main asteroid belt: between drawn Mars (1.88 * 2000 = 3760) and
	// Jupiter (11.86 * 2000 = 23724).
	initBelt(&asteroidBelt, "include/obj/asteroid.obj", 1500,
		4200.0f, 6800.0f, 120.0f, 6.0f, 30.0f);
	// Kuiper belt: past drawn Jupiter, kept inside the view frustum
	// (zFar = 100000, camera distance max 25000) — the specced "past
	// Neptune" range (Neptune draws at 329600) is beyond the far plane
	// and can never be seen.
	initBelt(&kuiperBelt, "include/obj/rock.obj", 1000,
		35000.0f, 95000.0f, 1200.0f, 15.0f, 80.0f);
}

void initComets()
{
	createShader(&cometShader, "src/shaders/comet.vert",
		"src/shaders/comet.frag");
	createShader(&tailShader, "src/shaders/tail.vert",
		"src/shaders/tail.frag");

	// Per-comet seeds. q (perihelion distance) sets how close each one dips to
	// the sun; rA (aphelion distance) sets how far out the ellipse arcs before
	// looping back. dir aims each perihelion into the camera-facing hemisphere
	// (world +z, ahead of the default camera at +5000) so the bright near-sun
	// pass happens in FRONT of the sun rather than behind it, from a different
	// side each time. phase is the starting point along the orbit (a fraction
	// of PI, i.e. a fraction of the half orbit) so the three arrive at
	// different intervals, and kMult sets each orbit's tempo so they never
	// re-sync into a lockstep pass.
	// Perihelion distance q: kept close to the sun (glow is 3000) so the pass
	// visibly dives into the sun. Each perihelion keeps z>0 (in front of the
	// sun, between it and the default camera at z=5000) so it is seen crossing
	// the sun's face rather than hidden behind it.
	static const float seedQ[]  = { 1500.0f, 2600.0f, 3400.0f };
	// Aphelion distance rA: arcs far out into the system (the Kuiper belt
	// draws at 35000..95000 and the far plane is 100000), then returns on the
	// closed ellipse. Larger rA = more eccentric, longer hidden far stretch.
	static const float seedRA[] = { 46000.0f, 38000.0f, 58000.0f };
	// Perihelion world angle. z = q*sin(dir) stays in (0, 5000):
	//   #0 right  (1336, 705)   #1 left   (-2256, 1756)   #2 right (3205, 1166)
	static const float seedDir[]   = {   0.5f,   2.4f,     0.35f  };
	static const float seedPhase[] = {  0.80f,  0.30f,  -0.45f };
	static const float seedKMult[] = {  1.00f,  1.35f,  0.75f };

	for(int ci = 0; ci < COMET_COUNT; ci++)
	{
	comet *c = &comets[ci];
	c->mesh = ObjLoadModel("include/obj/rock.obj");
	c->particleCount = COMET_PARTICLES;

	// Ellipse geometry, sun at the focus: e = (rA - q)/(rA + q), p = q(1+e).
	// r(theta) = p / (1 + e cos(theta)); theta spans the full -PI..+PI, with
	// perihelion at 0 and aphelion at +/-PI.
	c->q = seedQ[ci];
	c->e = (seedRA[ci] - seedQ[ci]) / (seedRA[ci] + seedQ[ci]);
	c->p = seedQ[ci] * (1.0f + c->e);
	c->dir = seedDir[ci];
	c->theta = seedPhase[ci] * 3.14159265358979f; // stagger the three comets
	c->orbitK = COMET_ORBIT_K * seedKMult[ci];
	c->spin = beltRand() * 6.2832f;

	// Nucleus VAO: positions then normals (same layout as initBelt).
	glGenVertexArrays(1, &c->vao);
	glBindVertexArray(c->vao);
	glGenBuffers(1, &c->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, c->vbo);
	glBufferData(GL_ARRAY_BUFFER, c->mesh.size + c->mesh.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, c->mesh.size, c->mesh.points);
	glBufferSubData(GL_ARRAY_BUFFER, c->mesh.size, c->mesh.nsize, c->mesh.normals);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(c->mesh.size));

	// Tail: a shared billboard quad (two triangles, corners in -0.5..0.5)
	// instanced once per particle.
	static const float quad[] = {
		-0.5f, -0.5f,   0.5f, -0.5f,   0.5f, 0.5f,
		-0.5f, -0.5f,   0.5f,  0.5f,  -0.5f, 0.5f
	};
	glGenVertexArrays(1, &c->tailVao);
	glBindVertexArray(c->tailVao);
	glGenBuffers(1, &c->quadVbo);
	glBindBuffer(GL_ARRAY_BUFFER, c->quadVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), BUFFER_OFFSET(0));

	// Per-particle instance data: aFracJit (frac, jx, jy) + aSizeAlpha.
	int n = c->particleCount;
	float *inst = malloc(sizeof(float) * 5 * n);
	for(int i = 0; i < n; i++)
	{
		float frac = (float)i / (float)(n - 1);
		// Sprites are large and dense near the head (the coma) and thin out
		// toward the tip; alpha falls off the same way (see tail.vert).
		float size = (24.0f + 150.0f * (1.0f - frac)) * (0.7f + 0.6f * beltRand());
		inst[5*i + 0] = frac;
		inst[5*i + 1] = beltRand() * 2.0f - 1.0f;
		inst[5*i + 2] = beltRand() * 2.0f - 1.0f;
		inst[5*i + 3] = size;
		inst[5*i + 4] = 0.5f + 0.5f * beltRand();
	}
	glGenBuffers(1, &c->tailVbo);
	glBindBuffer(GL_ARRAY_BUFFER, c->tailVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 5 * n, inst, GL_STATIC_DRAW);
	free(inst);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), BUFFER_OFFSET(0));
	glVertexAttribDivisor(1, 1);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), BUFFER_OFFSET(3*sizeof(GLfloat)));
	glVertexAttribDivisor(2, 1);

	glBindVertexArray(0);
	updateComet(c, 1.0f);   // one reference-frame step to seed initial pose
	}
}

void updateComet(comet *c, float timeScale)
{
	// Ellipse, sun at the focus (origin): r = p / (1 + e cos theta), with
	// perihelion at theta = 0 and aphelion at theta = +/-PI. Advance theta by
	// K/r^2 per 60-Hz reference frame, scaled by timeScale:
	// equal-area-in-equal-time, so it whips around perihelion and crawls far
	// out, like a real comet, at a real-time constant pace regardless of fps.
	// The whole conic is rotated by c->dir so perihelion points along a chosen
	// world direction (kept near the default camera's line of sight).
	float ang = c->theta + c->dir;
	float r = c->p / (1.0f + c->e * cosf(c->theta));
	float dTheta = c->orbitK / (r * r);
	c->theta -= dTheta * timeScale;
	if(c->theta < -3.14159265358979f)
		c->theta += 6.28318530717959f; // wrap at aphelion: same point, no visible reset
	c->spin += 0.005f * timeScale;

	ang = c->theta + c->dir;
	r = c->p / (1.0f + c->e * cosf(c->theta));
	c->r = r;
	c->pos.x = r * cosf(ang);
	c->pos.y = 0.0f;
	c->pos.z = r * sinf(ang);

	// Tail: anti-sunward (the sun is at the origin), length and brightness
	// both proportional to 1/r^2, peaking at perihelion.
	c->tailDir.x = c->pos.x / r;
	c->tailDir.y = c->pos.y / r;
	c->tailDir.z = c->pos.z / r;
	float q = c->q;
	float falloff = (q / r) * (q / r);
	c->tailLen = COMET_TAIL_MAX * falloff;
	c->tailWidth = 0.18f * c->tailLen;
	c->tailAlpha = falloff;

	// Distance-based visibility (like a real comet): fully bright at the
	// perihelion distance q, smoothly faded to 0 beyond COMET_VIS_FAR * q.
	// The slow far-out crawl happens while invisible, so only the fast,
	// bright close pass is seen.
	float rFar = COMET_VIS_FAR * q;
	float t = (rFar - r) / (rFar - q);
	t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
	c->vis = t * t * (3.0f - 2.0f * t);
}

void drawComets()
{
	for(int ci = 0; ci < COMET_COUNT; ci++)
	{
	comet *c = &comets[ci];
	if(c->vis < 0.02f)
		continue;   // faded out on the far arc; skipped so the distant crawl is never drawn

	// Nucleus: alpha-faded by the pass-end window (uFade), writes depth.
	glUseProgram(cometShader);
	glUniform1f(glGetUniformLocation(cometShader, "uFade"), c->vis);
	mat4 m = multiplymat4(multiplymat4(translatevec3(c->pos), rotateY(c->spin)),
		scale(COMET_NUCLEUS_SCALE));
	initMVP(cometShader, m, v);
	glBindVertexArray(c->vao);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDrawArrays(GL_TRIANGLES, 0, c->mesh.vertexNumber);
	glDisable(GL_BLEND);
	glBindVertexArray(0);

	// Tail: additive camera-facing sprites; depth-tested so a closer planet
	// hides it, but no depth write so the sky behind stays visible.
	glUseProgram(tailShader);
	{
		float r = c->r > 0.0f ? c->r : 1.0f;
		float falloff = (c->q / r) * (c->q / r);
		glUniformMatrix4fv(glGetUniformLocation(tailShader, "projection"), 1, GL_FALSE, &p.m[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(tailShader, "view"), 1, GL_FALSE, &v.m[0][0]);
		glUniform3f(glGetUniformLocation(tailShader, "uNucleus"), c->pos.x, c->pos.y, c->pos.z);
		glUniform3f(glGetUniformLocation(tailShader, "uTailDir"),
			c->pos.x / r, c->pos.y / r, c->pos.z / r);
		glUniform1f(glGetUniformLocation(tailShader, "uTailLen"), COMET_TAIL_MAX * falloff);
		glUniform1f(glGetUniformLocation(tailShader, "uTailWidth"), 0.18f * COMET_TAIL_MAX * falloff);
		glUniform1f(glGetUniformLocation(tailShader, "uTailAlpha"), falloff * c->vis);
		glUniform3f(glGetUniformLocation(tailShader, "uColor"), 0.55f, 0.75f, 1.0f);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
		glDisable(GL_CULL_FACE);
		glDepthMask(GL_FALSE);

		glBindVertexArray(c->tailVao);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, c->particleCount);
		glBindVertexArray(0);

		glDepthMask(GL_TRUE);
		glEnable(GL_CULL_FACE);
		glDisable(GL_BLEND);
	}
	}
}

void initPlanet() {
	createShader(&planetShader, "src/shaders/planet.vert",
		"src/shaders/planet.frag");

	vec3 *tangent = malloc(sizeof(vec3) * planet.vertexNumber);
	generateTangents(planet.vertexNumber, planet.points, tangent);
	vec3 *vna = malloc(sizeof(vec3) * planet.vertexNumber);
	generateSmoothNormals(planet.vertexNumber, vna, planet.points, planet.normals);

	glGenVertexArrays(1, &planetVAO);
	glBindVertexArray(planetVAO);
	glGenBuffers(1, &planetVBO);
	glBindBuffer(GL_ARRAY_BUFFER, planetVBO);
	glBufferData(GL_ARRAY_BUFFER, planet.size + planet.nsize + sizeof(vec3) * planet.vertexNumber, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0,	planet.size, planet.points);
	glBufferSubData(GL_ARRAY_BUFFER, planet.size, planet.nsize, vna);
	glBufferSubData(GL_ARRAY_BUFFER, planet.size+planet.nsize, sizeof(vec3) * planet.vertexNumber,	tangent);

	vPosition = glGetAttribLocation(planetShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));

    vNormal = glGetAttribLocation(planetShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size));

    vTangent = glGetAttribLocation(planetShader, "vTangent");
    glEnableVertexAttribArray(vTangent);
    glVertexAttribPointer(vTangent, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size+planet.nsize));

	glBindVertexArray(0);

	free(tangent);
	free(vna);
}

void initPlanetRing()
{
	createShader(&ringShader, "src/shaders/ring.vert",
		"src/shaders/ring.frag");

	for(int i = 0; i < 11; i++)
	{
		if(!planetInstanceArray[i].createRing)
			continue;

		planetInstanceArray[i].ringTex = generateRingTexture(i);
		ringArray[i] = createRing(128, planetInstanceArray[i].ringInner, planetInstanceArray[i].ringOuter);

		glGenVertexArrays(1, &ringVAO[i]);
		glBindVertexArray(ringVAO[i]);
		glGenBuffers(1, &ringVBO[i]);
		glBindBuffer(GL_ARRAY_BUFFER, ringVBO[i]);
		glBufferData(GL_ARRAY_BUFFER, ringArray[i].size + ringArray[i].nsize + ringArray[i].texsize, NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, ringArray[i].size, ringArray[i].points);
		glBufferSubData(GL_ARRAY_BUFFER, ringArray[i].size, ringArray[i].nsize, ringArray[i].normals);
		glBufferSubData(GL_ARRAY_BUFFER, ringArray[i].size + ringArray[i].nsize, ringArray[i].texsize, ringArray[i].texCoords);

		GLint aPosition = glGetAttribLocation(ringShader, "vPosition");
		glEnableVertexAttribArray(aPosition);
		glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));

		GLint aNormal = glGetAttribLocation(ringShader, "vNormal");
		glEnableVertexAttribArray(aNormal);
		glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(ringArray[i].size));

		GLint aTexture = glGetAttribLocation(ringShader, "vTexture");
		glEnableVertexAttribArray(aTexture);
		glVertexAttribPointer(aTexture, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), BUFFER_OFFSET(ringArray[i].size + ringArray[i].nsize));
		glBindVertexArray(0);
	}
}

void initAtmosphere() {
	createShader(&atmosphereShader, "src/shaders/atmosphere.vert",
		"src/shaders/atmosphere.frag");

    glGenVertexArrays(1, &atmosphereVAO);
	glBindVertexArray(atmosphereVAO);
	glGenBuffers(1, &atmosphereVBO);
	glBindBuffer(GL_ARRAY_BUFFER, atmosphereVBO);
	glBufferData(GL_ARRAY_BUFFER, planet.size + planet.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, planet.size, planet.points);
	glBufferSubData(GL_ARRAY_BUFFER, planet.size, planet.nsize, planet.normals);
	vPosition = glGetAttribLocation(atmosphereShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));

    vNormal = glGetAttribLocation(atmosphereShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size));
	glBindVertexArray(0);
}

void initOrbits()
{
	createShader(&orbitShader, "src/shaders/orbit.vert",
		"src/shaders/orbit.frag");

	// Unit circle in the XZ plane (the plane every planet orbits in). All
	// planets share it; drawOrbits scales it to each planet's orbit radius.
	GLfloat verts[ORBIT_SEGMENTS*3];
	for(int i = 0; i < ORBIT_SEGMENTS; i++)
	{
		float a = 2.0f*3.14159265358979f*(float)i/(float)ORBIT_SEGMENTS;
		verts[i*3+0] = cosf(a);
		verts[i*3+1] = 0.0f;
		verts[i*3+2] = sinf(a);
	}

	glGenVertexArrays(1, &orbitVAO);
	glBindVertexArray(orbitVAO);
	glGenBuffers(1, &orbitVBO);
	glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	GLint aPosition = glGetAttribLocation(orbitShader, "vPosition");
	glEnableVertexAttribArray(aPosition);
	glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), (GLvoid*)0);
	glBindVertexArray(0);
}

void initSkybox() {
	GLfloat skyboxVertices[] = {
        // Positions
        -1.0f, 1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,

        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,

        -1.0f, 1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, 1.0f
    };

    createShader(&skyboxShader, "src/shaders/skybox.vert",
		"src/shaders/skybox.frag");

	glGenVertexArrays(1, &skyboxVAO);
	glBindVertexArray(skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    vPosition = glGetAttribLocation(skyboxShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT , GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glBindVertexArray(0);
}

void initFramebuffer() {
	int shadowWidth = 1024;
	int shadowHeight = 1024;
	glGenFramebuffers(1, &shadowFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowWidth, shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("Framebuffer not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initSunQuad()
{
	GLfloat quadVertices[] = {
		-1.0f, -1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f
	};

	glGenVertexArrays(1, &sunQuadVAO);
	glBindVertexArray(sunQuadVAO);
	glGenBuffers(1, &sunQuadVBO);
	glBindBuffer(GL_ARRAY_BUFFER, sunQuadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	GLint vPositionLoc = glGetAttribLocation(sunShader, "vPosition");
	glEnableVertexAttribArray(vPositionLoc);
	glVertexAttribPointer(vPositionLoc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glBindVertexArray(0);
}

// ---------------------------------------------------------------------------
// Loading screen
//
// Between window creation and the end of resource loading, main() decodes
// ~60 textures with SOIL and builds all shaders/VAOs before the first buffer
// swap, so the window sits blank and unresponsive the whole time. The screen
// itself needs no textures (nothing is loaded yet): a plain shader fills the
// viewport with a dark backdrop and a progress bar uProgress. redrawLoading
// is called once per discrete load step so the bar advances while the rest of
// the pipeline blocks, and glfwPollEvents inside it keeps the window
// responsive (Esc works) during loading.
// ---------------------------------------------------------------------------

void initLoadingScreen()
{
	createShader(&loadingShader, "src/shaders/loading.vert",
		"src/shaders/loading.frag");

	GLfloat quadVertices[] = {
		-1.0f, -1.0f, 0.0f,
		1.0f, -1.0f, 0.0f,
		1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f
	};

	glGenVertexArrays(1, &loadingVAO);
	glBindVertexArray(loadingVAO);
	glGenBuffers(1, &loadingVBO);
	glBindBuffer(GL_ARRAY_BUFFER, loadingVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	GLint vPositionLoc = glGetAttribLocation(loadingShader, "vPosition");
	glEnableVertexAttribArray(vPositionLoc);
	glVertexAttribPointer(vPositionLoc, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glBindVertexArray(0);
}

void renderLoadingScreen(float progress)
{
	glfwPollEvents();

	int fbw, fbh;
	glfwGetFramebufferSize(loadWindow, &fbw, &fbh);
	glViewport(0, 0, fbw, fbh);

	glDisable(GL_DEPTH_TEST);
	glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(loadingShader);
	glUniform1f(glGetUniformLocation(loadingShader, "uProgress"), progress);
	glUniform1f(glGetUniformLocation(loadingShader, "uTime"), (float)glfwGetTime());
	glBindVertexArray(loadingVAO);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);

	glfwSwapBuffers(loadWindow);
}

// One loading step: repaint the bar at the current fraction done. Call once
// per discrete unit of work (each texture decode, each init phase).
void finishLoadingStep()
{
	float pct = (float)loadStepDone / (float)LOAD_TOTAL_STEPS;
	loadStepDone++;
	if (pct > 1.0f)
		pct = 1.0f;
	renderLoadingScreen(pct);
}

void init()
{
	fprintf(stderr, "[trace] init: tetrahedron\n");
	planet = tetrahedron(5);
	//atmosphere = tetrahedron(5);
	createShader(&sunShader, "src/shaders/sun.vert",
		"src/shaders/sun.frag");
	initSunQuad();
	fprintf(stderr, "[trace] init: framebuffer\n");

    initFramebuffer();
    fprintf(stderr, "[trace] init: initPlanet\n");
    initPlanet();
	fprintf(stderr, "[trace] init: planetRing\n");
	initPlanetRing();
	fprintf(stderr, "[trace] init: skybox\n");
	initSkybox();
	fprintf(stderr, "[trace] init: object\n");
	initObject();
	fprintf(stderr, "[trace] init: belts\n");
	initBelts();
	fprintf(stderr, "[trace] init: comets\n");
	initComets();
	fprintf(stderr, "[trace] init: atmosphere\n");
	initAtmosphere();
	initOrbits();
	fprintf(stderr, "[trace] init: done\n");

    glEnable(GL_DEPTH_TEST);
}

void createPerspectiveMatrix()
{
	p = perspective(45.0f, ASPECT, zNear, zFar);
}

void setupLighting(int program)
{
    vec4 light_ambient = {0.2f, 0.2f, 0.2f, 1.0f};
    vec4 light_diffuse = {1.0f, 1.0f, 1.0f, 1.0f};
    vec4 light_specular = {1.0f, 1.0f, 1.0f, 1.0f};

    vec4 material_ambient = {0.2f, 0.2f, 1.0f, 1.0f};
    vec4 material_diffuse = {0.8f, 0.8f, 0.8f, 1.0f};
    vec4 material_specular = {0.5f, 0.5f, 0.5f, 1.0f};

    vec4 light_position = {0.0f, 0.0f, 1.0f, 1.0f};
    float material_shininess = 50.0f;

    vec4 ambient_product = multiplyvec4(light_ambient, material_ambient);
    vec4 diffuse_product = multiplyvec4(light_diffuse, material_diffuse);
    vec4 specular_product = multiplyvec4(light_specular, material_specular);

    glUniform4fv( glGetUniformLocation(program, "ambientProduct"), 1, (float*)(&ambient_product) );
    glUniform4fv( glGetUniformLocation(program, "diffuseProduct"), 1, (float*)(&diffuse_product) );
    glUniform4fv( glGetUniformLocation(program, "specularProduct"), 1, (float*)(&specular_product) );
	glUniform4fv( glGetUniformLocation(program, "lightPos"), 1, (float*)(&light_position) );
	glUniform1f ( glGetUniformLocation(program, "shininess"), material_shininess );
}

static int   mvpLastProg = -1, mvpLastFrame = -1;
static GLint mvpLProj, mvpLModel, mvpLView;

/* p/v are identical for every call in a frame; only m differs. Keying the
   cache on (program, frame) uploads projection/view once per program per
   frame. Never drop the frame key: a mid-frame program switch must
   refetch, or the cached locations would belong to another program. */
void initMVP(int shader, mat4 m, mat4 v)
{
	if (shader != mvpLastProg || gFrame != mvpLastFrame)
	{
		mvpLProj   = glGetUniformLocation( shader, "projection" );
		mvpLView   = glGetUniformLocation( shader, "view" );
		mvpLModel  = glGetUniformLocation( shader, "model" );
		glUniformMatrix4fv(mvpLProj, 1, GL_FALSE, &p.m[0][0]);
		glUniformMatrix4fv(mvpLView, 1, GL_FALSE, &v.m[0][0]);
		mvpLastProg  = shader;
		mvpLastFrame = gFrame;
	}
	glUniformMatrix4fv( mvpLModel, 1, GL_FALSE, &m.m[0][0] );
}

void drawSkybox(skyboxTexture)
{
	glUseProgram(skyboxShader);

	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);

    mat4 mv = getViewRotation();

	glUniformMatrix4fv(glGetUniformLocation( skyboxShader, "projection" ), 1, GL_TRUE, &p.m[0][0]);
    glUniformMatrix4fv(glGetUniformLocation( skyboxShader, "ModelView" ), 1, GL_TRUE, &mv.m[0][0] );

    glBindVertexArray (skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
}

void drawBelts()
{
	glUseProgram(beltShader);
	glUniformMatrix4fv(glGetUniformLocation(beltShader, "projection"), 1, GL_FALSE, &p.m[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(beltShader, "view"), 1, GL_FALSE, &v.m[0][0]);
	glUniform1f(glGetUniformLocation(beltShader, "uTime"), beltTime);

	belt *belts[2] = { &asteroidBelt, &kuiperBelt };
	for(int i = 0; i < 2; i++)
	{
		glBindVertexArray(belts[i]->vao);
		glDrawArraysInstanced(GL_TRIANGLES, 0, belts[i]->mesh.vertexNumber, belts[i]->count);
		glBindVertexArray(0);
	}
}

void bindTexture(GLuint activeTex, GLuint tex) {
    glActiveTexture(activeTex);
    glBindTexture(GL_TEXTURE_2D, tex);
}

// Shared uniforms for both sun passes (same billboard, same projected centre
// and half-size). uDisc picks the fragment-shader mode: 1 = opaque disc,
// 0 = additive corona halo.
static void setupSunUniforms(GLfloat uDisc)
{
	glUniformMatrix4fv(glGetUniformLocation( sunShader, "projection" ), 1, GL_FALSE, &p.m[0][0]);
	glUniformMatrix4fv(glGetUniformLocation( sunShader, "view" ), 1, GL_FALSE, &v.m[0][0]);
	glUniform1f(glGetUniformLocation(sunShader, "sunRadius"), SUN_GLOW_RADIUS);
	glUniform1f(glGetUniformLocation(sunShader, "systemTime"), (float)glfwGetTime());

	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	glUniform1f(glGetUniformLocation(sunShader, "aspect"), (float)viewport[2] / (float)viewport[3]);

	bindTexture(GL_TEXTURE0, sunTexture);
	bindTexture(GL_TEXTURE1, sunNormal);
	glUniform1i(glGetUniformLocation(sunShader, "tex"), 0);
	glUniform1i(glGetUniformLocation(sunShader, "normalTex"), 1);
	glUniform1f(glGetUniformLocation(sunShader, "uDisc"), uDisc);
}

// Fullscreen quad corners are in perimeter order (BL,BR,TR,TL). Draw as a fan,
// which tiles that ordering into two non-overlapping CCW triangles; a strip in
// perimeter order leaves the top-left sector uncovered.
static void drawSunQuadPass(void)
{
	glBindVertexArray(sunQuadVAO);
	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
	glBindVertexArray(0);
}

// Opaque sun sphere. Drawn BEFORE the planets, with depth writes on, so its
// stored depth makes it occlude any planet that orbits behind it. A planet in
// front is closer, so it still (correctly) draws over the disc.
void drawSunDisc()
{
	glUseProgram(sunShader);
	setupSunUniforms(1.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	drawSunQuadPass();
}

// Additive corona halo. Drawn LAST: depth-tested against the scene (so a
// passing planet still hides it) but never writes depth, so planets behind the
// halo stay visible and the glow simply blends over them.
void drawSun()
{
	glUseProgram(sunShader);
	setupSunUniforms(0.0f);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_FALSE);

	drawSunQuadPass();

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

void drawAtmosphere()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glUseProgram(atmosphereShader);
	//setupLighting(atmosphereShader);


	for(int i = 0; i < 11; i++)
    {
     	if(planetInstanceArray[i].createAtmosphere) {
			float scaleFactor = atmosphereArray[i].scaleFactor;
			float fOuter = planetInstanceArray[i].size*scaleFactor;
			float fInner = (planetInstanceArray[i].size);
			if (fOuter <= fInner) {
				// no real shell (e.g. atmosphere scale not set); skip
				continue;
			}
			//translation.x = planetInstanceArray[2].radius * cos(orbitSpeedArray[2]);
			//translation.y = 0.0;
			//translation.z = planetInstanceArray[2].radius * sin(orbitSpeedArray[2]);

			mat4 modelmat = planetTransform[i];
			mat4 tv = transposemat4(multiplymat4(modelmat, getViewPosition()));
			vec4 camMult = {-tv.m[3][0], -tv.m[3][1], -tv.m[3][2], -tv.m[3][3]};
			vec4 camPosition = multiplymat4vec4(tv, camMult);

			//mat4 m = multiplymat4(translatevec3(translation), scale());
			mat4 m = multiplymat4(modelmat, scale(planetInstanceArray[i].size*scaleFactor));
			// Per-body solar brightness + Rayleigh tint (set in atmosphereBuilder()).
			// The white Mie term is summed on top, so C_R's RGB sets each limb's hue.
			vec3 C_R = atmosphereArray[i].C_R;
			float E = atmosphereArray[i].E;

			glUniform1f(glGetUniformLocation(atmosphereShader, "fInnerRadius"), fInner);
			glUniform1f(glGetUniformLocation(atmosphereShader, "fOuterRadius"), fOuter);
			glUniform3f(glGetUniformLocation(atmosphereShader, "camPosition"), camPosition.x, camPosition.y, camPosition.z);
			glUniform3f(glGetUniformLocation(atmosphereShader, "C_R"), C_R.x, C_R.y, C_R.z);
			glUniform1f(glGetUniformLocation(atmosphereShader, "E"), E);
			glUniform1f(glGetUniformLocation(atmosphereShader, "time"), (float)glfwGetTime());

			initMVP(atmosphereShader, m, v);

			glBindVertexArray (atmosphereVAO);
			// The dome is an outward-facing sphere. When the camera is inside the
			// outer atmosphere every visible dome triangle is a back face, so
			// swap the cull face (the paper inverts the sky dome's front faces
			// for the camera-in-atmosphere case). The sky is a pure additive
			// pass and must not write depth, or its surface would hide the
			// moon/objects beyond the shell behind it.
			float camDistSq = camPosition.x*camPosition.x + camPosition.y*camPosition.y + camPosition.z*camPosition.z;
			glDepthMask(GL_FALSE);
			if (camDistSq < fOuter*fOuter) {
				glCullFace(GL_FRONT);
			}
			glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber);
			glCullFace(GL_BACK);
			glDepthMask(GL_TRUE);
			glBindVertexArray(0);
		}
    }
    glDisable(GL_BLEND);
}

void drawPlanet()
{
	//drawAtmosphere(translation, planetInstanceArray[2].size, 1.025);

	glUseProgram(planetShader);
	setupLighting(planetShader);
	for(int i = 0; i < 11; i++)
	{
		translation.x = planetInstanceArray[i].radius * (float)cos(orbitSpeedArray[i]);
		translation.y = 0.0;
		translation.z = planetInstanceArray[i].radius * (float)sin(orbitSpeedArray[i]);
		mat4 matT = multiplymat4(translatevec3(translation), scale(planetInstanceArray[i].size));
		mat4 matR = multiplymat4(rotateY(rotationSpeedArray[i]), rotateX(planetInstanceArray[i].axialTilt+45.0f));
		mat4 m = multiplymat4(matT,matR);
		planetTransform[i] = translatevec3(translation);

		initMVP(planetShader, m, v);

    	glBindVertexArray (planetVAO);
    	bindTexture(GL_TEXTURE0, planetInstanceArray[i].texture);
    	bindTexture(GL_TEXTURE1, planetInstanceArray[i].normal);
    	bindTexture(GL_TEXTURE2, planetInstanceArray[i].displacement);
    	glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
    	glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
    	glUniform1i(glGetUniformLocation(planetShader, "depthMap"), 2);
    	glUniformMatrix4fv( glGetUniformLocation(planetShader, "translation" ), 1, GL_FALSE, &matT.m[0][0] );
    	glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber);
    	glBindVertexArray(0);
    }
}

void drawOrbits()
{
	glUseProgram(orbitShader);

	// Faint white lines, the standard astronomy-app look. Depth is tested so
	// planet bodies hide the far side of an orbit, but the lines never write
	// depth, so they don't occlude anything themselves.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	glUniform1f(glGetUniformLocation(orbitShader, "uAlpha"), 0.4f);

	for(int i = 0; i < 11; i++)
	{
		// Same circle drawPlanet() orbits on: XZ plane, planet's radius as scale.
		mat4 m = scale(planetInstanceArray[i].radius);
		initMVP(orbitShader, m, v);

		glBindVertexArray(orbitVAO);
		glDrawArrays(GL_LINE_LOOP, 0, ORBIT_SEGMENTS);
		glBindVertexArray(0);
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}

void drawPlanetRing()
{
	glUseProgram(ringShader);

	for(int i = 0; i < 11; i++)
	{
		if(planetInstanceArray[i].createRing) {
			// Additive glow like the atmosphere, but alpha-weighted so the band
			// gaps (e.g. the Cassini division) show the planet through. Depth
			// is tested but not written: the planet drawn earlier already hides
			// the far ring, and the ring is a pure overlay in front of it.
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glDisable(GL_CULL_FACE);
			glDepthMask(GL_FALSE);

			mat4 modelmat = planetTransform[i];
			mat4 m = multiplymat4(multiplymat4(modelmat, rotateX(planetInstanceArray[i].axialTilt+45.0f)),
				scale(planetInstanceArray[i].size));

			initMVP(ringShader, m, v);
			glUniform3f(glGetUniformLocation(ringShader, "planetCenter"),
				modelmat.m[0][3], modelmat.m[1][3], modelmat.m[2][3]);
			glUniform1f(glGetUniformLocation(ringShader, "planetRadius"), planetInstanceArray[i].size);

			glBindVertexArray(ringVAO[i]);
			bindTexture(GL_TEXTURE0, planetInstanceArray[i].ringTex);
			glUniform1i(glGetUniformLocation(ringShader, "tex"), 0);
			glDrawArrays( GL_TRIANGLES, 0, ringArray[i].vertexNumber);
			glBindVertexArray(0);

			glEnable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
		}
	}
}

void drawMoon()
{
	glUseProgram(planetShader);
	setupLighting(planetShader);
	int c = 0;
	for(int i = 0; i < 11; i++)
	{
		for(int j = 0; j < planetInstanceArray[i].numMoons; j++)
		{
			translation.x = (moonInstanceArray[c].radius + planetInstanceArray[i].size) * (float)cos(moonOrbitSpeedArray[c]);
			translation.y = 0.0;
			translation.z = (moonInstanceArray[c].radius + planetInstanceArray[i].size) * (float)sin(moonOrbitSpeedArray[c]);

			mat4 m = multiplymat4(multiplymat4(planetTransform[i], translatevec3(translation)), scale(moonInstanceArray[c].size));

			initMVP(planetShader, m, v);

			glBindVertexArray (planetVAO);
			bindTexture(GL_TEXTURE0, moonInstanceArray[c].texture);
			bindTexture(GL_TEXTURE1, moonInstanceArray[c].normal);
			bindTexture(GL_TEXTURE2, moonDisplacementTexture);
			glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
			glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
			glUniform1i(glGetUniformLocation(planetShader, "depthMap"), 2);
			glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber );
			c++;
		}
	}
	glBindVertexArray(0);
}

#ifdef _WIN32
static GLADapiproc gladGetProcAddress(const char *name)
{
	GLADapiproc p = (GLADapiproc)wglGetProcAddress(name);
	if (!p)
		p = (GLADapiproc)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
	return p;
}
#endif

void doMovement()
{
	/* Any WASD press releases follow: the camera moves freely from wherever
	   it currently is. Dragging never releases follow. */
	if (actionPress == GLFW_PRESS
		&& (keys == GLFW_KEY_W || keys == GLFW_KEY_S
			|| keys == GLFW_KEY_A || keys == GLFW_KEY_D)
		&& followTarget != UI_ACT_NONE)
	{
		followTarget = UI_ACT_NONE;
		focusDolly = 0;
	}

	if(keys == GLFW_KEY_W && actionPress == GLFW_PRESS)
        processKeyboard(FORWARD, deltaTime);
    if(keys == GLFW_KEY_S && actionPress == GLFW_PRESS)
        processKeyboard(BACKWARD, deltaTime);
    if(keys == GLFW_KEY_A && actionPress == GLFW_PRESS)
        processKeyboard(LEFT, deltaTime);
    if(keys == GLFW_KEY_D && actionPress == GLFW_PRESS)
        processKeyboard(RIGHT, deltaTime);
}

GLFWwindow *setupGLFW() {
	fprintf(stderr, "[trace] glfw: init\n");
	if (!glfwInit())
	{
		fprintf(stderr, "[trace] glfw: init FAILED\n");
		glfwTerminate();
		exit(1);
	}
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 2);

	fprintf(stderr, "[trace] glfw: creating window\n");
	/* Bordered fullscreen: normal decorated window, maximized after
	 * creation. Aspect is taken from the actual (maximized) size. */
	int winW = (int)WIDTH, winH = (int)HEIGHT;
	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode *mode = (monitor != NULL) ? glfwGetVideoMode(monitor) : NULL;
	if (mode != NULL)
	{
		winW = mode->width;
		winH = mode->height;
	}
	fprintf(stderr, "[trace] glfw: window %dx%d\n", winW, winH);
	GLFWwindow* window = glfwCreateWindow(winW, winH, "SolarSystem", NULL, NULL);
	if (!window)
	{
		fprintf(stderr, "[trace] glfw: window creation FAILED\n");
		glfwTerminate();
		exit(1);
	}
	glfwMaximizeWindow(window);
	{
		int ww, wh;
		glfwGetWindowSize(window, &ww, &wh);
		if (ww > 0 && wh > 0)
			ASPECT = (float)ww / (float)wh;
		fprintf(stderr, "[trace] glfw: maximized %dx%d, aspect %.3f\n", ww, wh, ASPECT);
	}
	glfwMakeContextCurrent(window);
#ifndef __APPLE__
	if (!gladLoadGL(gladGetProcAddress))
	{
		fprintf(stderr, "[trace] gl: gladLoadGL FAILED\n");
		glfwTerminate();
		exit(1);
	}
	fprintf(stderr, "[trace] gl: version = %s\n", (const char *)glGetString(GL_VERSION));
	glfwSwapInterval(1);
#endif
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_ENABLED);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);
	return window;
}

int initCubemap()
{
	char *cubemapArray[] = {
		"include/textures/skybox3/right.png",
		"include/textures/skybox3/left.png",
		"include/textures/skybox3/top.png",
		"include/textures/skybox3/bottom.png",
		"include/textures/skybox3/back.png",
		"include/textures/skybox3/front.png"
	};
	return loadCubemap(cubemapArray);
}

void setupUIButtons()
{
	for (int i = 0; i < UI_TARGET_COUNT; i++)
	{
		uiAddButton(uiTargets[i].actId, uiTargets[i].icon, uiTargets[i].activeIcon);
		finishLoadingStep();
	}
}

/* ---------------------------------------------------------------------------
 * Moon panels
 *
 * Every planet that owns at least one moon gets a collapsible panel hung off
 * its right-edge button column: a small chevron toggle sits just to the
 * planet's left, and (when the toggle is active) a row of moon buttons
 * extends further left from it. Clicking a moon button makes the camera
 * follow that moon, exactly like the planet buttons.
 *
 * The chevrons are generated procedurally (no texture asset), and each moon
 * button reuses the moon's ALREADY-LOADED surface texture as its icon, so no
 * extra SOIL decode is needed and the icon always matches the body.
 * ------------------------------------------------------------------------- */

static GLuint moonChevronLeft, moonChevronRight;

/* Squared distance from point (px,py) to segment (ax,ay)-(bx,by). Used to
   rasterise the chevron as two thick strokes. */
static float uiSegDistSq(float px, float py,
                         float ax, float ay, float bx, float by)
{
    float abx = bx - ax, aby = by - ay;
    float len2 = abx * abx + aby * aby;
    float t = len2 > 0.0f ? ((px - ax) * abx + (py - ay) * aby) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float cx = ax + t * abx - px;
    float cy = ay + t * aby - py;
    return cx * cx + cy * cy;
}

/* dir < 0 -> chevron pointing left "<" (collapsed, content is to the left);
   dir > 0 -> chevron pointing right ">" (expanded, click to collapse). */
#define CHEVRON_SIZE 32
static GLuint createChevronTexture(int dir)
{
    unsigned char px[CHEVRON_SIZE * CHEVRON_SIZE * 4];
    for (int i = 0; i < CHEVRON_SIZE * CHEVRON_SIZE; i++)
        px[i * 4 + 3] = 0;   // start fully transparent

    /* Two strokes meeting at a tip. Default = left-pointing "<". */
    float tipX = 9.0f, midY = 16.0f;
    float wingX = 24.0f, topY = 8.0f, botY = 24.0f;
    if (dir > 0)              // mirror to point right ">"
    {
        tipX = (float)CHEVRON_SIZE - 1.0f - tipX;
        wingX = (float)CHEVRON_SIZE - 1.0f - wingX;
    }
    const float half = 3.0f, half2 = half * half;   // stroke half-thickness

    for (int y = 0; y < CHEVRON_SIZE; y++)
    {
        for (int x = 0; x < CHEVRON_SIZE; x++)
        {
            float pxx = (float)x + 0.5f, pyy = (float)y + 0.5f;
            float dTop  = uiSegDistSq(pxx, pyy, tipX, midY, wingX, topY);
            float dBot  = uiSegDistSq(pxx, pyy, tipX, midY, wingX, botY);
            float d = dTop < dBot ? dTop : dBot;
            if (d <= half2)
            {
                int o = (y * CHEVRON_SIZE + x) * 4;
                px[o + 0] = 120;   // pale blue-grey chevron
                px[o + 1] = 150;
                px[o + 2] = 200;
                px[o + 3] = 255;
            }
        }
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, CHEVRON_SIZE, CHEVRON_SIZE, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

void setupUIMoonButtons()
{
    moonChevronLeft  = createChevronTexture(-1);
    moonChevronRight = createChevronTexture(+1);

    /* First global index of each planet's (contiguous) moon block inside
       moonInstanceArray. Defs are added in planet order, so a planet's moons
       are consecutive; moonStart[p] is the first one. */
    int moonStart[11];
    for (int p = 0; p < 11; p++)
        moonStart[p] = -1;
    for (int c = 0; c < loadedMoonCount; c++)
    {
        int p = moonInstanceArray[c].planet;
        if (moonStart[p] < 0)
            moonStart[p] = c;
    }

    for (int p = 0; p < 11; p++)
    {
        if (planetInstanceArray[p].numMoons <= 0)
            continue;

        /* Collapsible toggle, always visible. Collapsed icon = "<", expanded
           (active) icon = ">" via the button's active-texture swap. */
        uiAddButtonTex(UI_ACT_TOGGLE_BASE + p, p,
                       moonChevronLeft, moonChevronRight, 1);
        finishLoadingStep();

        /* One moon button per moon, hidden until the panel is expanded. */
        for (int j = 0; j < planetInstanceArray[p].numMoons; j++)
        {
            int m = moonStart[p] + j;
            uiAddButtonTex(UI_ACT_MOON_BASE + m, p,
                           moonInstanceArray[m].texture, 0, 0);
            finishLoadingStep();
        }
    }
}

/* Camera sliders (bottom-left, blue = movement speed on top, orange =
   scroll speed below). Log-mapped so the low end stays usable:
   t in [0,1] -> min*(max/min)^t. */
#define UI_SLIDER_MOVEMENT 0
#define UI_SLIDER_SCROLL   1
#define UI_SLIDER_COUNT    2

#define CAMERA_MOVEMENT_MIN 100.0f
#define CAMERA_MOVEMENT_MAX 20000.0f
#define CAMERA_SCROLL_MIN 50.0f
#define CAMERA_SCROLL_MAX 6000.0f

static float cameraSliderValue(float minV, float maxV, float t)
{
	return minV * powf(maxV / minV, t);
}

static float cameraSliderInitT(float minV, float maxV, float value)
{
	return logf(value / minV) / logf(maxV / minV);
}

void onCameraSliderChanged(int id, float t)
{
	if (id == UI_SLIDER_MOVEMENT)
		MovementSpeed = cameraSliderValue(CAMERA_MOVEMENT_MIN, CAMERA_MOVEMENT_MAX, t);
	else if (id == UI_SLIDER_SCROLL)
		CameraScrollSpeed = cameraSliderValue(CAMERA_SCROLL_MIN, CAMERA_SCROLL_MAX, t);
}

/* Accent colours double as labels: blue = movement, orange = scroll. */
static const float sliderAccentMovement[3] = { 0.45f, 0.65f, 1.0f };
static const float sliderAccentScroll[3] = { 1.0f, 0.55f, 0.30f };

void uiSetupCameraSliders()
{
	uiSliderSetChangedFn(onCameraSliderChanged);
	/* t0 puts each handle at the current camera default. */
	uiAddSlider(UI_SLIDER_MOVEMENT,
		cameraSliderInitT(CAMERA_MOVEMENT_MIN, CAMERA_MOVEMENT_MAX, MovementSpeed),
		sliderAccentMovement);
	uiAddSlider(UI_SLIDER_SCROLL,
		cameraSliderInitT(CAMERA_SCROLL_MIN, CAMERA_SCROLL_MAX, CameraScrollSpeed),
		sliderAccentScroll);
}

/* Same position formula drawPlanet uses each frame. */
vec3 getPlanetWorldPosition(int i)
{
	vec3 p;
	p.x = planetInstanceArray[i].radius * (float)cos(orbitSpeedArray[i]);
	p.y = 0.0f;
	p.z = planetInstanceArray[i].radius * (float)sin(orbitSpeedArray[i]);
	return p;
}

/* Same position formula drawMoon uses each frame: the planet's centre plus
   the moon's offset around it (orbit radius = moon.radius + planet.size,
   angle = moonOrbitSpeedArray). Returns world space. */
vec3 getMoonWorldPosition(int m)
{
	int p = moonInstanceArray[m].planet;
	float r = moonInstanceArray[m].radius + planetInstanceArray[p].size;
	vec3 off;
	off.x = r * (float)cos(moonOrbitSpeedArray[m]);
	off.y = 0.0f;
	off.z = r * (float)sin(moonOrbitSpeedArray[m]);
	return addvec3(getPlanetWorldPosition(p), off);
}

/* Eases the arcball pivot to the followed body, and (while focusDolly is
   set, right after a button click) dollies CameraDistance to a comfortable
   frame for that body. Exponential smoothing, so it is frame-rate
   independent. A followed body recomputes its goal from the current orbit
   angles each frame, so tracking a moving moon works exactly like tracking
   a planet. A paused followed body is stationary — the pivot still
   converges to it, which is fine. */
void updateCameraFollow()
{
	if (followTarget == UI_ACT_NONE)
		return;

	vec3 goal;
	float goalDist;
	if (followTarget == UI_ACT_SUN)
	{
		goal.x = 0.0f;
		goal.y = 0.0f;
		goal.z = 0.0f;
		goalDist = 5000.0f;
	}
	else if (followTarget >= UI_ACT_MOON_BASE)
	{
		int m = followTarget - UI_ACT_MOON_BASE;
		goal = getMoonWorldPosition(m);
		goalDist = moonInstanceArray[m].size * 3.0f;
	}
	else
	{
		goal = getPlanetWorldPosition(followTarget);
		goalDist = planetInstanceArray[followTarget].size * 3.0f;
	}

	float k = 1.0f - expf(-6.0f * deltaTime);
	vec3 curPivot = getCameraPivot();
	vec3 next;
	next.x = curPivot.x + (goal.x - curPivot.x) * k;
	next.y = curPivot.y + (goal.y - curPivot.y) * k;
	next.z = curPivot.z + (goal.z - curPivot.z) * k;
	setCameraPivot(next);

	if (focusDolly)
	{
		if (goalDist < 10.0f)
			goalDist = 10.0f;
		if (goalDist > CameraDistanceMax)
			goalDist = CameraDistanceMax;

		CameraDistance += (goalDist - CameraDistance) * k;
		if (fabsf(CameraDistance - goalDist) < 0.02f * goalDist)
			focusDolly = 0;
	}
}

int main(int argc, char *argv[])
{
	setvbuf(stderr, NULL, _IONBF, 0);
	fprintf(stderr, "[trace] main: start\n");
#ifdef __APPLE__
	chdir("/Users/tjgreen/Documents/OpenGL/Sol");
#else
	{
		char exePath[MAX_PATH];
		GetModuleFileNameA(NULL, exePath, MAX_PATH);
		for (char *p = exePath + strlen(exePath) - 1; p > exePath; p--)
		{
			if (*p == '\\' || *p == '/')
			{
				*p = '\0';
				break;
			}
		}
		_chdir(exePath);
	}
#endif
	{
		char cwd[MAX_PATH];
#ifdef _WIN32
		GetCurrentDirectoryA(sizeof(cwd), cwd);
#else
		getcwd(cwd, sizeof(cwd));
#endif
		fprintf(stderr, "[trace] main: cwd = %s\n", cwd);
	}

	fprintf(stderr, "[trace] main: creating window\n");
	GLFWwindow *window = setupGLFW();
	fprintf(stderr, "[trace] main: window ready\n");

	// Show the loading screen before the first blocking load, so the window
	// never sits blank/unresponsive while textures are decoded.
	loadWindow = window;
	initLoadingScreen();
	loadStepDone = 0;
	renderLoadingScreen(0.0f);

	fprintf(stderr, "[trace] main: loading skybox\n");
	GLuint skyboxTexture = initCubemap();
	finishLoadingStep();
	fprintf(stderr, "[trace] main: skybox loaded\n");

	//GLFWwindow* window2 = glfwCreateWindow(500, 500, "SolarSystem", NULL, NULL);
	//glfwMakeContextCurrent(window2);

	/*Cross platform compatibility stuff uncomment if not on mac
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
  		printf(stderr, "Error: %s\n", glewGetErrorString(err));
	}*/

	fprintf(stderr, "[trace] main: loading sun textures\n");
	sunTexture = loadTexture("include/textures/Planets/sun2.jpg");
	finishLoadingStep();
	sunNormal = loadTexture("include/textures/Planets/sunNormal.png");
	finishLoadingStep();
	fprintf(stderr, "[trace] main: planetBuilder\n");
	planetBuilder(); // steps inside its texture loops
	fprintf(stderr, "[trace] main: moonBuilder\n");
	moonBuilder(); // steps inside
	fprintf(stderr, "[trace] main: atmosphereBuilder\n");
	atmosphereBuilder();
	finishLoadingStep();
	fprintf(stderr, "[trace] main: init\n");
	init();
	finishLoadingStep();
	fprintf(stderr, "[trace] main: perspective\n");
	createPerspectiveMatrix();

	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	glCullFace(GL_BACK);

	attachGUIShaders();
	finishLoadingStep();
	uiInit();
	setupUIButtons();
	setupUIMoonButtons();
	uiSetupCameraSliders();
	{
		int ww, wh;
		glfwGetWindowSize(window, &ww, &wh);
		uiLayoutColumn(ww, wh, UI_TARGET_COUNT);
		uiLayoutPanels(ww, wh);
		uiLayoutSliders(ww, wh, UI_SLIDER_COUNT);
	}
	// Some moons may be missing on disk, so the count above can land below
	// 100%; force one final full-bar frame before handing over to the scene.
	renderLoadingScreen(1.0f);
	loadWindow = NULL;
	fprintf(stderr, "[trace] main: enter render loop\n");

	float fpsFrames= 0;
	float lastTime = 0;
	while(!glfwWindowShouldClose(window))
	{
		GLfloat currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        gFrame++;
        updateCameraFollow();
        v = getViewMatrix();

        fpsFrames++;
		if(currentFrame - lastTime >= 1.0)
		{
			//printf("%f\n", 1000/fpsFrames);
			fpsFrames = 0;
			lastTime += 1.0;
		}

		glfwPollEvents();
		doMovement();

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		{
			int fbw, fbh;
			glfwGetFramebufferSize(window, &fbw, &fbh);
			if (fbw > 0 && fbh > 0)
				glViewport(0, 0, fbw, fbh);
		}

		drawSkybox(skyboxTexture);
		// Opaque sun sphere first: it writes depth, so it occludes any planet
		// that orbits behind it (a closer planet still draws over the disc).
		drawSunDisc();
		drawPlanet();
		drawOrbits();
		drawPlanetRing();
		drawBelts();
		//glFrontFace(GL_CW);
		drawAtmosphere();
		//glFrontFace(GL_CCW);
		drawComets();
		drawMoon();
		// Corona halo last: additive, no depth write, so it blends over
		// anything behind it while still being occluded by anything in front.
		drawSun();

		/* UI last: no depth test, standard alpha blend, its own state. */
		uiDrawAll(followTarget, stopRotation);

		if(stopRotation == 0){
			// Frame-rate independent motion: the tuning constants below are
			// per-60-Hz-frame steps; timeScale = 60*deltaTime advances them
			// by the correct amount of real time at any fps. Clamped so the
			// first frame (deltaTime spans the whole texture load) and long
			// stalls advance a few steps, not a teleport.
			float timeScale = deltaTime * 60.0f;
			if (timeScale > 4.0f)
				timeScale = 4.0f;
			for(int i = 0; i < 11; i++)
			{
				orbitSpeedArray[i] += 0.001f/planetInstanceArray[i].orbit * timeScale;
			}
			for(int i = 0; i < 11; i++)
			{
				rotationSpeedArray[i] += 0.001f/planetInstanceArray[i].day * timeScale;
			}
			for(int i = 0; i < loadedMoonCount; i++)
			{
				moonOrbitSpeedArray[i] += 0.0001f/moonInstanceArray[i].orbit * timeScale;
			}
			beltTime += timeScale;
			for(int ci = 0; ci < COMET_COUNT; ci++)
				updateComet(&comets[ci], timeScale);
			theta += 0.009f * timeScale;
		}

		glfwSwapBuffers(window);
	}

	glDeleteVertexArrays(1, &planetVAO);
    glDeleteBuffers(1, &planetVBO);
    for(int i = 0; i < 11; i++)
    {
    	if(planetInstanceArray[i].createRing)
    	{
			glDeleteVertexArrays(1, &ringVAO[i]);
			glDeleteBuffers(1, &ringVBO[i]);
			free(ringArray[i].points);
			free(ringArray[i].normals);
			free(ringArray[i].texCoords);
			glDeleteTextures(1, &planetInstanceArray[i].ringTex);
		}
	}

	glDeleteVertexArrays(1, &asteroidBelt.vao);
	glDeleteBuffers(1, &asteroidBelt.vbo);
	glDeleteBuffers(1, &asteroidBelt.ibo);
	glDeleteVertexArrays(1, &kuiperBelt.vao);
	glDeleteBuffers(1, &kuiperBelt.vbo);
	glDeleteBuffers(1, &kuiperBelt.ibo);

	for(int ci = 0; ci < COMET_COUNT; ci++)
	{
		glDeleteVertexArrays(1, &comets[ci].vao);
		glDeleteBuffers(1, &comets[ci].vbo);
		glDeleteVertexArrays(1, &comets[ci].tailVao);
		glDeleteBuffers(1, &comets[ci].quadVbo);
		glDeleteBuffers(1, &comets[ci].tailVbo);
		free(comets[ci].mesh.points);
		free(comets[ci].mesh.normals);
	}
	glDeleteProgram(cometShader);
	glDeleteProgram(tailShader);

	uiShutdown();
	/* Chevron icons are shared, caller-owned textures (owned == 0 in the
	   buttons), so uiShutdown doesn't free them — delete them here. The moon
	   buttons reference moonInstanceArray textures, which (as before) are not
	   freed at shutdown. */
	if (moonChevronLeft)
		glDeleteTextures(1, &moonChevronLeft);
	if (moonChevronRight)
		glDeleteTextures(1, &moonChevronRight);
	glfwTerminate();
	return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

	if(action == GLFW_PRESS)
    	actionPress = GLFW_PRESS;
    else if(action == GLFW_RELEASE)
    	actionPress = 0;

    if (key == GLFW_KEY_W && action == GLFW_PRESS){
    	keys = GLFW_KEY_W;
    	doMovement();
    }
    if (key == GLFW_KEY_S && action == GLFW_PRESS){
    	keys = GLFW_KEY_S;
    	doMovement();
    }
    if (key == GLFW_KEY_A && action == GLFW_PRESS){
    	keys = GLFW_KEY_A;
    	doMovement();
    }
    if (key == GLFW_KEY_D && action == GLFW_PRESS){
    	keys = GLFW_KEY_D;
    	doMovement();
    }
    if(action == GLFW_RELEASE)
    	keys = 0;
    if (key == GLFW_KEY_R && action == GLFW_PRESS){
		if(stopRotation == 0)
			stopRotation = 1;
		else
			stopRotation = 0;
    }
}


void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	uiSetMouse((float)xpos, (float)ypos);
	int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	if (state == GLFW_PRESS)
	{
		processMouseMovement(window, (float)xpos, (float)ypos);
	}
}

/* A press on a button acts on the button and must NOT also start an arcball
   drag; only empty-space presses start rotation. A drag that releases over a
   button does nothing, because only the press is tested. */
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		double mx, my;
		glfwGetCursorPos(window, &mx, &my);
		int id = uiHitTest((float)mx, (float)my);
		if (id == UI_ACT_PAUSE)
		{
			stopRotation = stopRotation ? 0 : 1;
		}
		else if (id >= UI_ACT_TOGGLE_BASE)
		{
			/* Collapse/expand this planet's moon row (toggles are 200+planet). */
			int p = id - UI_ACT_TOGGLE_BASE;
			uiSetPlanetExpanded(p, !uiPlanetExpanded(p));
		}
		else if (id >= UI_ACT_MOON_BASE)
		{
			/* Follow the clicked moon (moons are 100+index). */
			followTarget = id;
			focusDolly = 1;
		}
		else if (id == UI_ACT_SUN)
		{
			followTarget = UI_ACT_SUN;
			focusDolly = 1;
		}
		else if (id >= UI_ACT_PLANET_BASE)
		{
			followTarget = id;
			focusDolly = 1;
		}
		else if (uiSliderPress((float)mx, (float)my) >= 0)
		{
			/* grabbed a camera slider; do not start an arcball drag */
		}
		else
			processMousePress(window);
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		int slid = uiSliderRelease();
		if (slid == UI_SLIDER_MOVEMENT)
			fprintf(stderr, "[trace] ui: movement speed = %g\n", (double)MovementSpeed);
		else if (slid == UI_SLIDER_SCROLL)
			fprintf(stderr, "[trace] ui: scroll speed = %g\n", (double)CameraScrollSpeed);
		processMouseRelease();
	}
}

void window_size_callback(GLFWwindow* window, int width, int height)
{
	uiLayoutColumn(width, height, UI_TARGET_COUNT);
	uiLayoutPanels(width, height);
	uiLayoutSliders(width, height, UI_SLIDER_COUNT);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	processMouseScroll((GLfloat)yoffset);
}
