#include "Common.h"
#include "Main.h"
#include "Bodies.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <stdio.h>
#include <math.h>

// Heliocentric ecliptic mean longitude (deg) at J2000 and mean motion
// (deg/day) for each body, matching planet order (0 Mercury .. 10 Eris).
// Rates are 360/period-in-days; L0 anchors each orbit to the real sky at
// startup. Planet longitudes are mean (elliptical correction omitted), which
// is within a few degrees of the true value for this visual model.
static const float planetLon0Deg[PLANET_COUNT] = {
	252.25084f,  // Mercury
	181.97973f,  // Venus
	100.46435f,  // Earth
	355.45332f,  // Mars
	152.00000f,  // Ceres
	34.40438f,   // Jupiter
	49.94432f,   // Saturn
	313.23810f,  // Uranus
	304.88003f,  // Neptune
	238.92903f,  // Pluto
	8.22000f     // Eris
};
static const float planetLonRateDegPerDay[PLANET_COUNT] = {
	4.092338f,   // Mercury (87.969 d)
	1.602130f,   // Venus   (224.701 d)
	0.985647f,   // Earth   (365.256 d)
	0.524027f,   // Mars    (686.980 d)
	0.214115f,   // Ceres   (1681.65 d)
	0.083084f,   // Jupiter (4332.59 d)
	0.033468f,   // Saturn  (10759.2 d)
	0.011731f,   // Uranus  (30688.5 d)
	0.005981f,   // Neptune (60182 d)
	0.003975f,   // Pluto   (90560 d)
	0.00176025f  // Eris    (204516 d)
};
#define DEG_TO_RAD 0.017453292519943295
#define TAU 6.283185307179586
// Base moon angular rate: the old per-frame step was 0.0001/moon.orbit rad;
// times OLD_TICKS_PER_DAY rad-per-day gives 0.00172023/moon.orbit.
#define MOON_BASE_RAD_PER_DAY 0.00172023
// Base axial-spin rate (rad per sim-day): the old per-frame step was
// 0.001/planet.day; times OLD_TICKS_PER_DAY gives rad-per-day.
#define SPIN_BASE_RAD_PER_DAY (0.001f * OLD_TICKS_PER_DAY)

// Historical radius/size units for the builders (meters-ish scale), scaled
// up 1000x: Eris orbits at 1.36e8 world units. The double-precision,
// camera-relative pipeline (LinearAlg dvec3, floating-origin rendering)
// resolves position detail at this scale; the per-frame relative offsets
// handed to the GPU stay float.
static float planetScaleMult = 100000.0;
static float planetRadMult = 2000000.0;
static float moonRadMult = 500000.0;

// File-scope data tables and per-frame derived angles.
static planetParameters planetInstanceArray[PLANET_COUNT];
static atmosphereParameters atmosphereArray[PLANET_COUNT];
static moonParameters moonInstanceArray[MAX_MOONS];
static float orbitSpeedArray[PLANET_COUNT] = {0};
static float rotationSpeedArray[PLANET_COUNT] = {0};
static float moonOrbitSpeedArray[MAX_MOONS] = {0};
static int loadedMoonCount = 0;
static GLuint moonNormalTexture;
static GLuint moonDisplacementTexture;

// Normalize an angle (radians) into [0, TAU). simDays grows without bound in
// a long-lived run, so reducing it before storing keeps the angles as small
// floats instead of huge ones.
static float normAngle2pi(double a)
{
	double r = a - TAU * floor(a / TAU);
	if (r < 0.0)
		r += TAU;
	if (r >= TAU)
		r -= TAU;
	return (float)r;
}

static int *loadPlanetTextures(void (*step)(void), int planetTexArray[])
{
	char *textureStrings[] = {"assets/textures/planets/mercury.jpg",
		"assets/textures/planets/venus.png",
		"assets/textures/planets/earth.jpg",
		"assets/textures/planets/mars.jpg",
		"assets/textures/planets/ceres.jpg",
		"assets/textures/planets/jupiter.jpg",
		"assets/textures/planets/saturn.png",
		"assets/textures/planets/uranus.jpg",
		"assets/textures/planets/neptune.jpg",
		"assets/textures/planets/pluto.png",
		"assets/textures/planets/eris.jpg"};
	int arraySize = sizeof(textureStrings)/sizeof(textureStrings[0]);
	for(int i = 0; i < arraySize; i++)
	{
		planetTexArray[i] = loadTexture(textureStrings[i]);
		step();
	}
	return planetTexArray;
}

static int *loadPlanetNormals(void (*step)(void), int planetTexArray[])
{
	char *textureStrings[] = {"assets/textures/planets/mercuryNormal.png",
		"assets/textures/planets/venusNormal.png",
		"assets/textures/planets/earthNormal.jpg",
		"assets/textures/planets/marsNormal.png",
		"assets/textures/planets/ceresNormal.png",
		"assets/textures/planets/jupiterNormal.png",
		"assets/textures/planets/saturnNormal.png",
		"assets/textures/planets/uranusNormal.png",
		"assets/textures/planets/neptuneNormal.png",
		"assets/textures/planets/plutoNormal.png",
		"assets/textures/planets/erisNormal.png"};
	int arraySize = sizeof(textureStrings)/sizeof(textureStrings[0]);
	for(int i = 0; i < arraySize; i++)
	{
		planetTexArray[i] = loadTexture(textureStrings[i]);
		step();
	}
	return planetTexArray;
}

/* Height maps for vertex displacement are DERIVED from the normal maps: the
   tangent-space slopes n = 2*tex-1 give dh/du = -nx/nz, dh/dv = -ny/nz, and
   integrating those gives height up to a per-path constant. Two path
   integrals (along rows, along columns) are averaged to spread each one's
   drift; the row integral also subtracts its linear residual so the seam at
   u=0/u=1 closes. Result is normalized to [0,1] into an R8 texture
   (mipmapped, repeat in S). On decode failure a 1x1 mid-gray texture
   returns (flat surface). lodBase = log2(texelWidth/PI) is the mip a
   full-face quadtree patch should fetch; the vertex shader adds log2(cell)
   to it. */
#define HEIGHT_NZ_MIN 0.05f

static float heightLodBases[PLANET_COUNT];
static const char *planetNormalPaths[PLANET_COUNT] = {
	"assets/textures/planets/mercuryNormal.png",
	"assets/textures/planets/venusNormal.png",
	"assets/textures/planets/earthNormal.jpg",
	"assets/textures/planets/marsNormal.png",
	"assets/textures/planets/ceresNormal.png",
	"assets/textures/planets/jupiterNormal.png",
	"assets/textures/planets/saturnNormal.png",
	"assets/textures/planets/uranusNormal.png",
	"assets/textures/planets/neptuneNormal.png",
	"assets/textures/planets/plutoNormal.png",
	"assets/textures/planets/erisNormal.png"};

static GLuint buildHeightFromNormal(const char *path, int index)
{
	int w = 0, h = 0, chan = 0;
	unsigned char *img = SOIL_load_image(path, &w, &h, &chan, SOIL_LOAD_RGB);
	if (!img || w < 2 || h < 2) {
		if (img)
			SOIL_free_image_data(img);
		fprintf(stderr, "[trace] heightFromNormal: no pixels for '%s', flat 1x1\n", path);
		unsigned char mid = 128;
		GLuint tex;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, &mid);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glBindTexture(GL_TEXTURE_2D, 0);
		heightLodBases[index] = 0.0f;
		return tex;
	}

	int n = w * h;
	float *hR = (float *)malloc(sizeof(float) * n);  /* row integral */
	float *hC = (float *)malloc(sizeof(float) * n);  /* column integral */
	float *slopes = (float *)malloc(sizeof(float) * n);

	for(int i = 0; i < h; i++) {
		for(int j = 0; j < w; j++) {
			int p = (i * w + j) * 3;
			float nx = 2.0f * img[p + 0] / 255.0f - 1.0f;
			float ny = 2.0f * img[p + 1] / 255.0f - 1.0f;
			float nz = 2.0f * img[p + 2] / 255.0f - 1.0f;
			if (nz < HEIGHT_NZ_MIN)
				nz = HEIGHT_NZ_MIN;
			hC[i * w + j] = ny / nz;        /* dh/dv, integrated in place below */
			slopes[i * w + j] = -nx / nz;   /* dh/du, integrated in place below */
		}
	}
	SOIL_free_image_data(img);

	for(int i = 0; i < h; i++) {
		float acc = 0.0f;
		for(int j = 0; j < w; j++)
			hR[i * w + j] = (acc += slopes[i * w + j]);
		float resid = acc;
		for(int j = 0; j < w; j++)
			hR[i * w + j] -= resid * (float)(j + 1) / (float)w;
	}
	for(int j = 0; j < w; j++) {
		float acc = 0.0f;
		for(int i = 0; i < h; i++)
			hC[i * w + j] = (acc += hC[i * w + j]);
	}
	free(slopes);

	float hmin = 1e30f, hmax = -1e30f;
	for(int k = 0; k < n; k++) {
		hR[k] += hC[k];
		if (hR[k] < hmin) hmin = hR[k];
		if (hR[k] > hmax) hmax = hR[k];
	}
	free(hC);

	float span = hmax - hmin;
	unsigned char *bytes = (unsigned char *)malloc(n);
	for(int k = 0; k < n; k++) {
		float t = (span > 0.0f) ? (hR[k] - hmin) / span : 0.5f;
		bytes[k] = (unsigned char)(t * 255.0f + 0.5f);
	}
	free(hR);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, bytes);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	free(bytes);

	heightLodBases[index] = log2f((float)w / 3.14159265f);
	return tex;
}

static int *buildPlanetHeightMaps(void (*step)(void), int planetTexArray[])
{
	int arraySize = sizeof(planetNormalPaths)/sizeof(planetNormalPaths[0]);
	for(int i = 0; i < arraySize; i++)
	{
		planetTexArray[i] = buildHeightFromNormal(planetNormalPaths[i], i);
		step();
	}
	return planetTexArray;
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
// would remap to N = (-1,-1,-1)), and a black 1x1 displacement is a safe
// height source for the (moon-inactive) vertex-displacement fetch.
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

void Bodies_InitPlanets(void (*step)(void))
{
	planetParameters mercury, venus, earth, mars, eris, jupiter, saturn, uranus, neptune, pluto, ceres;
	int numTextures = 11;
	int planetTexArray[11];
	*planetTexArray = *loadPlanetTextures(step, planetTexArray);

	int planetNormArray[11];
	*planetNormArray = *loadPlanetNormals(step, planetNormArray);

	int planetDisplacementArray[11];
	*planetDisplacementArray = *buildPlanetHeightMaps(step, planetDisplacementArray);

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
	saturn.radius = 9.5f*planetRadMult; saturn.size = 9.45f*planetScaleMult; saturn.orbit = 29.4571f; saturn.axialTilt = 26.73f; saturn.day = 0.439583f; saturn.createAtmosphere = 1; saturn.createRing = 1; saturn.ringInner = 1.24f; saturn.ringOuter = 2.27f;
	saturn.texture = planetTexArray[6]; saturn.normal = planetNormArray[6]; saturn.displacement = planetDisplacementArray[6];
	uranus.radius = 19.2f*planetRadMult; uranus.size = 4.007f*planetScaleMult; uranus.orbit = 84.0205f; uranus.axialTilt = 97.77f; uranus.day = 0.71833f; uranus.createAtmosphere = 1; uranus.createRing = 1; uranus.ringInner = 1.55f; uranus.ringOuter = 2.05f;
	uranus.texture = planetTexArray[7]; uranus.normal = planetNormArray[7]; uranus.displacement = planetDisplacementArray[7];
	neptune.radius = 30.1f*planetRadMult; neptune.size = 3.883f*planetScaleMult; neptune.orbit = 164.8f; neptune.axialTilt = 28.32f; neptune.day = 0.6713f; neptune.createAtmosphere = 1; neptune.createRing = 0;
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

	/* Vertex-displacement relief, fraction of planet radius. The height map is
	   derived by integrating the normal map, whose integration drift adds
	   broad low-frequency wrinkles on top of the real detail, so relief stays
	   at the per-mille level; large values crumple the whole body (oceans
	   included) into a wet-foil silhouette. Gas giants stay flat: their normal
	   maps are cloud detail. */
	static const float dispScaleTable[PLANET_COUNT] = {
		0.004f, 0.004f, 0.003f, 0.005f, 0.006f, 0.001f,
		0.001f, 0.001f, 0.001f, 0.005f, 0.005f };
	for (int i = 0; i < PLANET_COUNT; i++) {
		planetInstanceArray[i].dispScale = dispScaleTable[i];
		planetInstanceArray[i].dispLodBase = heightLodBases[i];
	}
}

void Bodies_InitMoons(void (*step)(void))
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
		{ "moon",      "assets/textures/planets/moon.jpg",          2,  1.0f,       0.272704f,  0.083f     },
		{ "phobos",    "assets/textures/moons/phobos.jpg",     3,  0.015609f,  0.001413f,  0.063014f  },
		{ "deimos",    "assets/textures/moons/deimos.jpg",     3,  0.037919f,  0.000306f,  0.003425f  },
		{ "callisto",  "assets/textures/moons/callisto.jpg",   5,  4.883117f,  0.376707f,  0.046575f  },
		{ "europa",    "assets/textures/moons/europa.jpg",     5,  1.742597f,  0.24329f,   0.009703f  },
		{ "ganymede",  "assets/textures/moons/ganymede.jpg",   5,  2.779221f,  0.206498f,  0.292123f  },
		{ "io",        "assets/textures/moons/io.jpg",         5,  0.680519f,  0.142936f,  0.004849f  },
		{ "dione",     "assets/textures/moons/dione.jpg",      6,  0.98026f,   0.088134f,  0.006849f  },
		{ "enceladus", "assets/textures/moons/enceladus.jpg",  6,  0.384179f,  0.024643f,  0.003753f  },
		{ "iapetus",   "assets/textures/moons/iapetus.jpg",    6,  35.224156f, 0.115461f,  0.216438f  },
		{ "mimas",     "assets/textures/moons/mimas.jpg",      6,  0.481919f,  0.010909f,  0.002581f  },
		{ "rhea",      "assets/textures/moons/rhea.jpg",       6,  1.368935f,  0.119918f,  0.012329f  },
		{ "tethys",    "assets/textures/moons/tethys.jpg",     6,  0.766234f,  0.04183f,   0.005171f  },
		{ "titan",     "assets/textures/moons/titan.png",      6,  3.173688f,  0.404175f,  0.043607f  },
		{ "ariel",     "assets/textures/moons/ariel.jpg",      7,  0.495844f,  0.0908f,    0.006545f  },
		{ "miranda",   "assets/textures/moons/miranda.jpg",    7,  0.336078f,  0.036886f,  0.003836f  },
		{ "oberon",    "assets/textures/moons/oberon.jpg",     7,  1.513247f,  0.1194f,    0.036886f  },
		{ "titania",   "assets/textures/moons/titania.jpg",    7,  1.132234f,  0.1235f,    0.023836f  },
		{ "umbriel",   "assets/textures/moons/umbriel.jpg",    7,  0.429151f,  0.091822f,  0.011342f  },
		{ "triton",    "assets/textures/moons/triton.jpg",     8,  0.921452f,  0.2122f,   -0.016101f  },
		{ "charon",    "assets/textures/moons/charon.jpg",     9,  0.045548f,  0.095f,     0.017479f  },
	};

	for(int i = 0; i < PLANET_COUNT; i++)
		planetInstanceArray[i].numMoons = 0;

	moonNormalTexture = createNeutralNormalTexture();
	step();
	moonDisplacementTexture = loadTexture("assets/textures/planets/moonDisplacement.png");
	step();

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
		step();
		m.normal = moonNormalTexture;
		moonInstanceArray[loadedMoonCount] = m;
		planetInstanceArray[defs[i].planet].numMoons++;
		loadedMoonCount++;
	}
	fprintf(stderr, "[trace] moonBuilder: %d/%d moons have textures\n", loadedMoonCount, MAX_MOONS);
}

void Bodies_InitAtmospheres(void)
{
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
	// H2/He with ammonia-crystal / H2S photochemical haze -> pale desaturated
	// gold, lighter and less saturated than Jupiter (the haze whitens the
	// bands). True flux ~0.16 (1/90 sun); boosted like the outer bodies so the
	// big haze envelope (scale height ~47 km, the largest of the planets)
	// still shows a limb glow at that distance.
	saturn.scaleFactor = 1.0600f; saturn.E = 0.40f; 	saturn.C_R.x = 1.00f; 	saturn.C_R.y = 0.95f; 	saturn.C_R.z = 0.83f;
	// H2/He/CH4, but the thickest photochemical haze of the ice giants: the
	// stagnant atmosphere lets the Aerosol-2 haze layer build up and whiten
	// the strong methane blue down to a pale low-saturation cyan. True flux
	// ~0.039 is invisible; boosted per the Pluto/Eris visibility precedent,
	// brightest of the outer trio because its haze is thickest and most
	// reflective.
	uranus.scaleFactor = 1.0350f; uranus.E = 1.6f; 	uranus.C_R.x = 0.62f; 	uranus.C_R.y = 0.86f; 	uranus.C_R.z = 1.00f;
	// H2/He/CH4 with a thinner haze than Uranus (turbulent convection churns
	// methane into the Aerosol-2 layer and precipitates it out as methane
	// snow), so Rayleigh scattering + methane red absorption dominate -> the
	// deep saturated azure. True flux ~0.016; boosted for visibility.
	neptune.scaleFactor = 1.0300f; neptune.E = 1.4f; 	neptune.C_R.x = 0.30f; 	neptune.C_R.y = 0.55f; 	neptune.C_R.z = 1.00f;
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

// Recompute every body's absolute angle from simDays (planets, spins, moons).
// All angles are pure functions of simDays, so changing speed never
// accumulates error.
void Bodies_Update(double simDays)
{
	int i;
	for (i = 0; i < PLANET_COUNT; i++)
	{
		orbitSpeedArray[i] = normAngle2pi(
			(planetLon0Deg[i] + planetLonRateDegPerDay[i] * simDays) * DEG_TO_RAD);
		rotationSpeedArray[i] = normAngle2pi(
			SPIN_BASE_RAD_PER_DAY / planetInstanceArray[i].day * simDays);
	}
	for (i = 0; i < loadedMoonCount; i++)
		moonOrbitSpeedArray[i] = normAngle2pi(
			MOON_BASE_RAD_PER_DAY / moonInstanceArray[i].orbit * simDays);
}

planetParameters *Bodies_Planets(void)
{
	return planetInstanceArray;
}

const moonParameters *Bodies_Moons(void)
{
	return moonInstanceArray;
}

int Bodies_MoonCount(void)
{
	return loadedMoonCount;
}

const atmosphereParameters *Bodies_Atmospheres(void)
{
	return atmosphereArray;
}

float Bodies_OrbitSpeed(int i)
{
	return orbitSpeedArray[i];
}

float Bodies_RotationSpeed(int i)
{
	return rotationSpeedArray[i];
}

float Bodies_MoonOrbitSpeed(int i)
{
	return moonOrbitSpeedArray[i];
}

GLuint Bodies_MoonDisplacementTexture(void)
{
	return moonDisplacementTexture;
}

dvec3 Bodies_PlanetWorldPosition(int i)
{
	dvec3 p;
	p.x = planetInstanceArray[i].radius * cos((double)orbitSpeedArray[i]);
	p.y = 0.0;
	p.z = planetInstanceArray[i].radius * sin((double)orbitSpeedArray[i]);
	return p;
}

dvec3 Bodies_MoonWorldPosition(int m)
{
	int p = moonInstanceArray[m].planet;
	double r = (double)moonInstanceArray[m].radius + (double)planetInstanceArray[p].size;
	dvec3 off;
	off.x = r * cos((double)moonOrbitSpeedArray[m]);
	off.y = 0.0;
	off.z = r * sin((double)moonOrbitSpeedArray[m]);
	return daddvec3(Bodies_PlanetWorldPosition(p), off);
}
