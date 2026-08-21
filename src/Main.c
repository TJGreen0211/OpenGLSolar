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
GLuint loadShader(char *vertex_path, char *fragement_path);
void initLoadingScreen();
void renderLoadingScreen(float progress);
void finishLoadingStep();
#ifdef _WIN32
static GLADapiproc gladGetProcAddress(const char *name);
#endif

int mousePosX, mousePosY;
imgButton button1, mercuryButton, venusButton, earthButton, marsButton,
	jupiterButton, saturnButton, uranusButton, neptuneButton;

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
GLuint vertUIShader, fragUIShader, UIShader;
GLuint vPosition, vNormal, vTangent, vBitangent;
GLuint planetVAO, planetVBO, skyboxVAO, skyboxVBO, objectVAO, objectVBO, atmosphereVAO, atmosphereVBO;
GLuint sunQuadVAO, sunQuadVBO;
GLuint ringVAO[11], ringVBO[11];
ring ringArray[11];
GLuint shadowFBO, depthMap, rboDepth;
GLuint ModelView, projection, model, view;
mat4 p, v;

GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;
int stopRotation = 0;

// Loading screen state. loadWindow is the GL context the loading frames are
// drawn on; it is set in main() right after setupGLFW() and read by the
// loader functions (planet/moon texture loops) so they can redraw the bar.
GLFWwindow *loadWindow = NULL;
GLuint loadingShader, loadingVAO, loadingVBO;
int loadStepDone = 0;


// Worst-case number of discrete load steps: skybox (1) + sun (2) +
// planet textures (11x3) + moons (neutral normal 1 + displacement 1 + up to
// MAX_MOONS textures) + atmosphere (1) + init (1) + buttons (1) + UI shader (1).
// If some moon textures are missing the bar tops out below 100% and the final
// frame forces it to 1.0, which is fine.
#define LOAD_TOTAL_STEPS (1 + 2 + 33 + 2 + MAX_MOONS + 4)

#define MAX_MOONS 21
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
	jupiter.radius = 5.2f*planetRadMult; jupiter.size = 11.209f*planetScaleMult; jupiter.orbit = 11.8618f; jupiter.axialTilt = 3.13f; jupiter.day = 0.413f; jupiter.createAtmosphere = 0; jupiter.createRing = 0;
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
	eris.radius = 68*planetRadMult; eris.size = 0.182f*planetScaleMult; eris.orbit = 558.04f; eris.axialTilt = 61.45f; eris.day = 1.079167f ; eris.createAtmosphere = 0; eris.createRing = 0;
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
	mercury.scaleFactor = 1.000f;mercury.E = 14.3f; 	mercury.C_R.x = 0.3f; 	mercury.C_R.y = 0.7f; 	mercury.C_R.z = 1.0f;
	venus.scaleFactor = 1.3008f; venus.E = 14.3f; 	venus.C_R.x = 0.3f; 	venus.C_R.y = 0.7f; 	venus.C_R.z = 1.0f;
	earth.scaleFactor = 1.0212f; earth.E = 14.3f; 	earth.C_R.x = 0.3f; 	earth.C_R.y = 0.7f; 	earth.C_R.z = 1.0f;
	mars.scaleFactor = 1.0210f; 	mars.E = 14.3f; 		mars.C_R.x = 0.3f; 	mars.C_R.y = 0.7f; 	mars.C_R.z = 1.0f;
	eris.scaleFactor = 1.001f; 	eris.E = 14.3f; 		eris.C_R.x = 0.3f; 	eris.C_R.y = 0.7f; 	eris.C_R.z = 1.0f;
	jupiter.scaleFactor = 1.1448f;jupiter.E = 14.3f; 	jupiter.C_R.x = 0.3f; 	jupiter.C_R.y = 0.7f; 	jupiter.C_R.z = 1.0f;
	saturn.scaleFactor = 0.0f; 	saturn.E = 14.3f; 	saturn.C_R.x = 0.3f; 	saturn.C_R.y = 0.7f; 	saturn.C_R.z = 1.0f;
	uranus.scaleFactor = 0.0f; 	uranus.E = 14.3f; 	uranus.C_R.x = 0.3f; 	uranus.C_R.y = 0.7f; 	uranus.C_R.z = 1.0f;
	neptune.scaleFactor = 0.0f; 	neptune.E = 14.3f; 	neptune.C_R.x = 0.3f; 	neptune.C_R.y = 0.7f; 	neptune.C_R.z = 1.0f;
	pluto.scaleFactor = 3.5000f; pluto.E = 14.3f; 	pluto.C_R.x = 0.3f; 	pluto.C_R.y = 0.7f; 	pluto.C_R.z = 1.0f;
	ceres.scaleFactor = 0.0f; 	ceres.E = 14.3f;		ceres.C_R.x = 0.3f; 	ceres.C_R.y = 0.7f; 	ceres.C_R.z = 1.0f;

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
		{ "phobos",    "include/textures/moons/mars/phobos.jpg",     3,  0.015609f,  0.001413f,  0.063014f  },
		{ "deimos",    "include/textures/moons/mars/deimos.jpg",     3,  0.037919f,  0.000306f,  0.003425f  },
		{ "callisto",  "include/textures/moons/jupiter/callisto.jpg",5,  4.883117f,  0.376707f,  0.046575f  },
		{ "europa",    "include/textures/moons/jupiter/europa.jpg",  5,  1.742597f,  0.24329f,   0.009703f  },
		{ "ganymede",  "include/textures/moons/jupiter/ganymede.jpg",5,  2.779221f,  0.206498f,  0.292123f  },
		{ "io",        "include/textures/moons/jupiter/io.jpg",      5,  0.680519f,  0.142936f,  0.004849f  },
		{ "dione",     "include/textures/moons/saturn/dione.jpg",    6,  0.98026f,   0.088134f,  0.006849f  },
		{ "enceladus", "include/textures/moons/saturn/enceladus.jpg",6,  0.384179f,  0.024643f,  0.003753f  },
		{ "iapetus",   "include/textures/moons/saturn/iapetus.jpg",  6,  35.224156f, 0.115461f,  0.216438f  },
		{ "mimas",     "include/textures/moons/saturn/mimas.jpg",    6,  0.481919f,  0.010909f,  0.002581f  },
		{ "rhea",      "include/textures/moons/saturn/rhea.jpg",     6,  1.368935f,  0.119918f,  0.012329f  },
		{ "tethys",    "include/textures/moons/saturn/tethys.jpg",   6,  0.766234f,  0.04183f,   0.005171f  },
		{ "titan",     "include/textures/moons/saturn/titan.png",    6,  3.173688f,  0.404175f,  0.043607f  },
		{ "ariel",     "include/textures/moons/uranus/ariel.jpg",    7,  0.495844f,  0.0908f,    0.006545f  },
		{ "miranda",   "include/textures/moons/uranus/miranda.jpg",  7,  0.336078f,  0.036886f,  0.003836f  },
		{ "oberon",    "include/textures/moons/uranus/oberon.jpg",   7,  1.513247f,  0.1194f,    0.036886f  },
		{ "titania",   "include/textures/moons/uranus/titania.jpg",  7,  1.132234f,  0.1235f,    0.023836f  },
		{ "umbriel",   "include/textures/moons/uranus/umbriel.jpg",  7,  0.429151f,  0.091822f,  0.011342f  },
		{ "triton",    "include/textures/moons/neptune/triton.jpg",  8,  0.921452f,  0.2122f,   -0.016101f  },
		{ "charon",    "include/textures/moons/pluto/charon.jpg",    9,  0.045548f,  0.095f,     0.017479f  },
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
	fprintf(stderr, "[trace] init: atmosphere\n");
	initAtmosphere();
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

void initMVP(int shader, mat4 m, mat4 v)
{
	glUniformMatrix4fv(glGetUniformLocation( shader, "projection" ), 1, GL_FALSE, &p.m[0][0]);
	glUniformMatrix4fv( glGetUniformLocation( shader, "model" ), 1, GL_FALSE, &m.m[0][0] );
	glUniformMatrix4fv( glGetUniformLocation( shader, "view" ), 1, GL_FALSE, &v.m[0][0] );
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

void drawObj()
{
	glUseProgram(planetShader);
	//mat4 rx = multiplymat4(translate(0.0, 0.0, -15.0), rotationSpace());
	mat4 m = scale(100);//rx;//multiplymat4(rx, rotateY(90));

	initMVP(planetShader, m, v);

	glBindVertexArray (objectVAO);
	//glDrawArrays( GL_TRIANGLES, 0, object.vertexNumber );
    glBindVertexArray(0);
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
			// 1/lambda^4 for lambda = (0.3, 0.7, 1.0) um, normalized so blue = 1
			// vec3 C_R = {1.0, 0.0337, 0.0081};
			vec3 C_R = {0.3f, 0.7f, 1.0f};
			float E = 14.3f;

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
	glfwWindowHint(GLFW_SAMPLES, 4);

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
#endif
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_ENABLED);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
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

void initializePlanetButtons()
{
	button1 = initButton(1300.0, 700.0, 100.0, &button1, "include/textures/buttonUp.png");
	mercuryButton = initButton(100.0, 100.0, 50.0, &mercuryButton, "include/textures/buttons/mercury.png");
	venusButton = initButton(200.0, 100.0, 50.0, &venusButton, "include/textures/buttons/venus.png");
	earthButton = initButton(300.0, 100.0, 50.0, &earthButton, "include/textures/buttons/earth.png");
	marsButton = initButton(400.0, 100.0, 50.0, &marsButton, "include/textures/buttons/mars.png");

	jupiterButton = initButton(500.0, 100.0, 50.0, &jupiterButton, "include/textures/buttons/jupiter.png");
	saturnButton = initButton(600.0, 100.0, 50.0, &saturnButton, "include/textures/buttons/saturn.png");
	uranusButton = initButton(700.0, 100.0, 50.0, &uranusButton, "include/textures/buttons/uranus.png");
	neptuneButton = initButton(800.0, 100.0, 50.0, &neptuneButton, "include/textures/buttons/neptune.png");
}

void drawPlanetButtons()
{
	drawButton(mercuryButton);
	drawButton(venusButton);
	drawButton(earthButton);
	drawButton(marsButton);
	drawButton(jupiterButton);
	drawButton(saturnButton);
	drawButton(uranusButton);
	drawButton(neptuneButton);
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

	initializePlanetButtons();
	finishLoadingStep();

	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	glCullFace(GL_BACK);
	attachGUIShaders();
	finishLoadingStep();
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

		glfwPollEvents();
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
		drawPlanetRing();
		//glFrontFace(GL_CW);
		drawAtmosphere();
		//glFrontFace(GL_CCW);
		drawObj();
		drawMoon();
		// Corona halo last: additive, no depth write, so it blends over
		// anything behind it while still being occluded by anything in front.
		drawSun();

		//drawButton(button1);
		//drawPlanetButtons();

		if(stopRotation == 0){
			for(int i = 0; i < 11; i++)
			{
				orbitSpeedArray[i] += 0.001f/planetInstanceArray[i].orbit;
			}
			for(int i = 0; i < 11; i++)
			{
				rotationSpeedArray[i] += 0.001f/planetInstanceArray[i].day;
			}
			for(int i = 0; i < loadedMoonCount; i++)
			{
				moonOrbitSpeedArray[i] += 0.0001f/moonInstanceArray[i].orbit;
			}
			theta += 0.009f;
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
	int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	if (state == GLFW_PRESS)
	{
		processMouseMovement(window, (float)xpos, (float)ypos);
	}
	mousePosX = (int)xpos;
	mousePosY = (int)ypos;
}

void checkButtonPress(imgButton b)
{
	if(mousePosX <= b.xTopRight && mousePosX >= b.xTopLeft && mousePosY >= b.yTopRight && mousePosY <= b.yBottomRight)
	{
		if(stopRotation == 0) {
			stopRotation = 1;
			buttonState(0);
		}
		else {
			stopRotation = 0;
			buttonState(1);
		}
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		checkButtonPress(button1);
		processMousePress(window);
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		processMouseRelease();
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	processMouseScroll((GLfloat)yoffset);
}
