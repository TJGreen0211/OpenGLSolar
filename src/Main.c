#include <stdio.h>
#include <math.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif
#include "Main.h"
#include "LoadingScreen.h"
#include "Bodies.h"
#include "SimTime.h"
#include "Belt.h"
#include "Comet.h"
#include "Skybox.h"
#include "Sun.h"
#include "Orbits.h"
#include "Planet.h"
#include "Rings.h"
#include "Moon.h"
#include "Atmosphere.h"


void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void window_size_callback(GLFWwindow* window, int width, int height);
GLuint loadShader(char *vertex_path, char *fragement_path);
#ifdef _WIN32
static GLADapiproc gladGetProcAddress(const char *name);
#endif

const float WIDTH = 1400, HEIGHT = 800;
float ASPECT = 1400.0f/800.0f;

sphere planet;
int keys;
int actionPress;
/* T toggles wireframe rendering of the scene geometry (planets, sun, belts,
   comets, rings, atmosphere shells); the skybox and UI keep fill mode. */
int wireframeMode = 0;

/* zNear stays small and fixed: the logarithmic depth buffer distributes its
   precision over log2(far/near), so a small near plane costs nothing, and a
   fixed one never clips geometry the camera flies into. zFar follows the
   camera so the frustum always bounds the whole system. */
float zNear = 0.05f, zFar = 100000.0f;

mat4 p, v;

GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;
int gFrame = 0;   // bumped once per render-loop iteration; keys the glSetupMVP cache
ViewMatrices appView;   // { proj, view } built once per frame for glSetupMVP

/* Camera follow state, set by the UI buttons. The value is the same space as
   a uiHitTest result (and a uiButton id), so each trackable body has one:
   UI_ACT_NONE (-3) = free camera (WASD moves it), UI_ACT_SUN (-1) = sun/
   overview pinned at the origin, 0..10 = Bodies_Planets()[i], 100+m =
   Bodies_Moons()[m]. Pause (-2) and toggles (200+) are controls, never
   targets. focusDolly runs the one-shot distance glide right after a target
   button is clicked. */
int followTarget = UI_ACT_NONE;
int focusDolly = 0;

/* One entry per button in the right-edge column, top to bottom: sun/
   overview, planets in Bodies_Planets() order (0..10), pause last.
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
	{ "assets/textures/buttons/sun.png",     NULL, UI_ACT_SUN },
	{ "assets/textures/buttons/mercury.png", NULL, 0 },
	{ "assets/textures/buttons/venus.png",   NULL, 1 },
	{ "assets/textures/buttons/earth.png",   NULL, 2 },
	{ "assets/textures/buttons/mars.png",    NULL, 3 },
	{ "assets/textures/buttons/ceres.png",   NULL, 4 },
	{ "assets/textures/buttons/jupiter.png", NULL, 5 },
	{ "assets/textures/buttons/saturn.png",  NULL, 6 },
	{ "assets/textures/buttons/uranus.png",  NULL, 7 },
	{ "assets/textures/buttons/neptune.png", NULL, 8 },
	{ "assets/textures/buttons/pluto.png",   NULL, 9 },
	{ "assets/textures/buttons/eris.png",    NULL, 10 },
	{ "assets/textures/buttonUp.png",        "assets/textures/buttonDown.png", UI_ACT_PAUSE },
};

// Recompute every body's absolute angle from simDays via Bodies, then set the
// belt clock and step the comets by dDays (belt-first, as before). Planets,
// spins, moons and the belt are pure functions of simDays; comets integrate
// their 1/r^2 anomaly, so they take the per-step dDays.
void updateBodies(float dDays)
{
	double d = SimTime_Days();
	Bodies_Update(d);
	Belt_SetSimTime(d);
	Comet_UpdateAll((double)dDays);
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

void init()
{
	fprintf(stderr, "[trace] init: tetrahedron\n");
	planet = tetrahedron(5);
	Sun_InitGL();
    fprintf(stderr, "[trace] init: initPlanet\n");
    Planet_InitGL(planet);
	fprintf(stderr, "[trace] init: planetRing\n");
	Rings_InitGL();
	fprintf(stderr, "[trace] init: skybox\n");
	Skybox_InitGL();
	fprintf(stderr, "[trace] init: belts\n");
	Belt_InitGL();
	fprintf(stderr, "[trace] init: comets\n");
	Comet_InitGL();
	fprintf(stderr, "[trace] init: atmosphere\n");
	Atmosphere_InitGL(planet);
	Orbits_InitGL();
	fprintf(stderr, "[trace] init: done\n");

    glEnable(GL_DEPTH_TEST);
    /* Logarithmic depth (see createPerspectiveMatrix + the *_DepthWrite*
       blocks in the shaders): the skybox lands exactly at depth 1.0 and the
       clear value is 1.0, so passes must use LEQUAL. */
    glDepthFunc(GL_LEQUAL);
}

void createPerspectiveMatrix()
{
	p = perspective(45.0f, ASPECT, zNear, zFar);
	glSetLogDepth(zNear, zFar);
}

/* Rebuild the projection each frame with a far plane that bounds the whole
   system from the camera (plus margin). Log depth makes the far plane free
   in precision, and this keeps the orbit rings and distant Kuiper belt
   inside the frustum at any scale. */
void updateDynamicProjection()
{
	static double maxOrbit = 0.0;
	if (maxOrbit == 0.0)
	{
		const planetParameters *pp = Bodies_Planets();
		for (int i = 0; i < PLANET_COUNT; i++)
			if (pp[i].radius > maxOrbit)
				maxOrbit = pp[i].radius;
	}
	dvec3 cam = getCameraPosition();
	/* `far` is a windows.h macro — use farPlane. */
	double farPlane = sqrt(cam.x * cam.x + cam.y * cam.y + cam.z * cam.z)
	                + maxOrbit * 1.1 + 1000.0;
	zFar = (float)farPlane;
	createPerspectiveMatrix();
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
	/* Logarithmic depth wants as many depth bits as offered. */
	glfwWindowHint(GLFW_DEPTH_BITS, 24);

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
	if (!gladLoadGL(gladGetProcAddress))
	{
		fprintf(stderr, "[trace] gl: gladLoadGL FAILED\n");
		glfwTerminate();
		exit(1);
	}
	fprintf(stderr, "[trace] gl: version = %s\n", (const char *)glGetString(GL_VERSION));
	glfwSwapInterval(1);
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_ENABLED);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetWindowSizeCallback(window, window_size_callback);
	return window;
}


void setupUIButtons()
{
	for (int i = 0; i < UI_TARGET_COUNT; i++)
	{
		uiAddButton(uiTargets[i].actId, uiTargets[i].icon, uiTargets[i].activeIcon);
		LoadingScreen_Step();
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
       Bodies_Moons(). Defs are added in planet order, so a planet's moons
       are consecutive; moonStart[p] is the first one. */
    int moonStart[11];
    for (int p = 0; p < 11; p++)
        moonStart[p] = -1;
    for (int c = 0; c < Bodies_MoonCount(); c++)
    {
        int p = Bodies_Moons()[c].planet;
        if (moonStart[p] < 0)
            moonStart[p] = c;
    }

    for (int p = 0; p < 11; p++)
    {
        if (Bodies_Planets()[p].numMoons <= 0)
            continue;

        /* Collapsible toggle, always visible. Collapsed icon = "<", expanded
           (active) icon = ">" via the button's active-texture swap. */
        uiAddButtonTex(UI_ACT_TOGGLE_BASE + p, p,
                       moonChevronLeft, moonChevronRight, 1);
        LoadingScreen_Step();

        /* One moon button per moon, hidden until the panel is expanded. */
        for (int j = 0; j < Bodies_Planets()[p].numMoons; j++)
        {
            int m = moonStart[p] + j;
            uiAddButtonTex(UI_ACT_MOON_BASE + m, p,
                           Bodies_Moons()[m].texture, 0, 0);
            LoadingScreen_Step();
        }
    }
}

/* Camera sliders (bottom-left, blue = movement speed on top, orange =
   scroll speed below). Log-mapped so the low end stays usable:
   t in [0,1] -> min*(max/min)^t. */
#define UI_SLIDER_MOVEMENT 0
#define UI_SLIDER_SCROLL   1
#define UI_SLIDER_COUNT    2

#define CAMERA_MOVEMENT_MIN 100000.0f
#define CAMERA_MOVEMENT_MAX 20000000.0f
#define CAMERA_SCROLL_MIN 50000.0f
#define CAMERA_SCROLL_MAX 6000000.0f

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

	dvec3 goal;
	float goalDist;
	if (followTarget == UI_ACT_SUN)
	{
		goal.x = 0.0;
		goal.y = 0.0;
		goal.z = 0.0;
		goalDist = 5000000.0f;
	}
	else if (followTarget >= UI_ACT_MOON_BASE)
	{
		int m = followTarget - UI_ACT_MOON_BASE;
		goal = Bodies_MoonWorldPosition(m);
		goalDist = Bodies_Moons()[m].size * 3.0f;
	}
	else
	{
		goal = Bodies_PlanetWorldPosition(followTarget);
		goalDist = Bodies_Planets()[followTarget].size * 3.0f;
	}

	float k = 1.0f - expf(-6.0f * deltaTime);
	dvec3 curPivot = getCameraPivot();
	dvec3 next;
	next.x = curPivot.x + (goal.x - curPivot.x) * k;
	next.y = curPivot.y + (goal.y - curPivot.y) * k;
	next.z = curPivot.z + (goal.z - curPivot.z) * k;
	setCameraPivot(next);

	if (focusDolly)
	{
		if (goalDist < 10000.0f)
			goalDist = 10000.0f;
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
	LoadingScreen_Init(window);
	LoadingScreen_Render(0.0f);

	fprintf(stderr, "[trace] main: loading skybox\n");
	GLuint skyboxTexture = Skybox_LoadCube();
	LoadingScreen_Step();
	fprintf(stderr, "[trace] main: skybox loaded\n");
	fprintf(stderr, "[trace] main: loading sun textures\n");
	Sun_InitTextures(LoadingScreen_Step);
	fprintf(stderr, "[trace] main: planetBuilder\n");
	Bodies_InitPlanets(LoadingScreen_Step); // steps inside its texture loops
	fprintf(stderr, "[trace] main: moonBuilder\n");
	Bodies_InitMoons(LoadingScreen_Step); // steps inside
	fprintf(stderr, "[trace] main: atmosphereBuilder\n");
	Bodies_InitAtmospheres();
	LoadingScreen_Step();
	fprintf(stderr, "[trace] main: init\n");
	init();
	LoadingScreen_Step();
	fprintf(stderr, "[trace] main: perspective\n");
	createPerspectiveMatrix();

	// Start the sim clock at the current UTC date/time and prime every body
	// angle from it, so the scene opens on the real sky (planets at their
	// current heliocentric mean longitudes).
	SimTime_Init();
	updateBodies(0.0f);

	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	glCullFace(GL_BACK);

	attachGUIShaders();
	LoadingScreen_Step();
	uiInit();
	setupUIButtons();
	setupUIMoonButtons();
	uiSetupCameraSliders();
	// Sim-speed presets (1 day / 1 week / 1 year per second of real time).
	uiAddTextButton(UI_ACT_SPEED_BASE + 0, SimTime_PresetLabel(0));
	uiAddTextButton(UI_ACT_SPEED_BASE + 1, SimTime_PresetLabel(1));
	uiAddTextButton(UI_ACT_SPEED_BASE + 2, SimTime_PresetLabel(2));
	{
		int ww, wh;
		glfwGetWindowSize(window, &ww, &wh);
		uiLayoutColumn(ww, wh, UI_TARGET_COUNT);
		uiLayoutPanels(ww, wh);
		uiLayoutSliders(ww, wh, UI_SLIDER_COUNT);
		uiLayoutTextButtons(ww, wh);
	}
	// Some moons may be missing on disk, so the count above can land below
	// 100%; force one final full-bar frame before handing over to the scene.
	LoadingScreen_Finish();
	fprintf(stderr, "[trace] main: enter render loop\n");

	float fpsFrames= 0;
	float lastTime = 0;
	while(!glfwWindowShouldClose(window))
	{
		GLfloat currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        gFrame++;
		/* Apply input callbacks before taking this frame's camera snapshot.
		   Mouse drag changes both the view rotation and camera orbit position;
		   processing it after appView is built mixes the new camera-relative
		   positions with the previous frame's view matrix. */
		glfwPollEvents();
		doMovement();
        updateCameraFollow();
        updateDynamicProjection();
        v = getViewMatrix();
        appView.proj = p;
        appView.view = v;
        appView.frame = gFrame;

        fpsFrames++;
		if(currentFrame - lastTime >= 1.0)
		{
			//printf("%f\n", 1000/fpsFrames);
			fpsFrames = 0;
			lastTime += 1.0;
		}

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		{
			int fbw, fbh;
			glfwGetFramebufferSize(window, &fbw, &fbh);
			if (fbw > 0 && fbh > 0)
				glViewport(0, 0, fbw, fbh);
		}

		Skybox_Draw(skyboxTexture, &appView);
		/* Wireframe applies to the scene geometry only: the skybox stays
		   filled (a line cube would ruin the backdrop) and the UI text/
		   widgets below draw after the mode is restored. */
		glPolygonMode(GL_FRONT_AND_BACK, wireframeMode ? GL_LINE : GL_FILL);
		// Opaque sun sphere first: it writes depth, so it occludes any planet
		// that orbits behind it (a closer planet still draws over the disc).
		Sun_DrawDisc(&appView);
		Planet_Draw(&appView);
		Orbits_Draw(&appView);
		Rings_Draw(&appView);
		Belt_Draw(&appView);
		// Moons before the atmosphere dome: they are opaque depth-writing
		// geometry, and the dome's additive in-scatter must composite over
		// them the same way it does over planets (a moon inside a shell is
		// fogged by planet.frag's extinction pass using its parent's shell).
		Moon_Draw(&appView);
		Atmosphere_Draw(&appView);
		Comet_Draw(&appView);
		// Corona halo last: additive, no depth write, so it blends over
		// anything behind it while still being occluded by anything in front.
		Sun_DrawGlow(&appView);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		/* HUD clock: the current sim date, its offset from J2000, and the
		   active speed preset (or PAUSED). uiDrawText sets up its own shader
		   state, so it runs just before the widget pass below. */
		{
			int yy, mo, dd, hh, mi, ss;
			SimTime_JulianDateToCalendar(SimTime_Days() + 2451545.0, &yy, &mo, &dd, &hh, &mi, &ss);
			char dateBuf[64];
			char offBuf[64];
			char spdBuf[48];
			sprintf(dateBuf, "%04d-%02d-%02d %02d:%02d:%02d", yy, mo, dd, hh, mi, ss);
			sprintf(offBuf, "J2000 %+.1f D", SimTime_Days());
			if (!SimTime_Paused())
				sprintf(spdBuf, "%d  %s", SimTime_PresetIndex() + 1, SimTime_PresetLabel(SimTime_PresetIndex()));
			else
				strcpy(spdBuf, "PAUSED");
			uiDrawText(dateBuf, 24.0f, 24.0f, 2.0f, 1.0f, 1.0f, 1.0f, 0.95f);
			uiDrawText(offBuf, 24.0f, 44.0f, 1.6f, 0.72f, 0.78f, 0.90f, 0.90f);
			uiDrawText(spdBuf, 24.0f, 64.0f, 1.6f, 0.85f, 0.85f, 0.85f, 0.85f);
			for (int si = 0; si < SimTime_PresetCount(); si++)
				uiSetTextButtonSelected(UI_ACT_SPEED_BASE + si, SimTime_PresetIndex() == si);
		}

		/* UI last: no depth test, standard alpha blend, its own state. */
		uiDrawAll(followTarget, SimTime_Paused());

		if(!SimTime_Paused()){
			// The sim clock (days since J2000) advances by the active speed
			// preset times the frame delta. Every body position is recomputed
			// absolutely from simDays in updateBodies, so it is frame-rate
			// independent and speed changes never accumulate error. Clamped so
			// the first frame (deltaTime spans the whole texture load) and long
			// stalls advance a few days, not a teleport.
			float dDays = (float)(SimTime_DaysPerSec() * deltaTime);
			if (dDays > 10.0f)
				dDays = 10.0f;
			if (dDays > 0.0f)
			{
				SimTime_Advance(dDays);
				updateBodies(dDays);
			}
		}

		glfwSwapBuffers(window);
	}

	Planet_Shutdown();
	Rings_Shutdown();

	Belt_Shutdown();
	Comet_Shutdown();

	uiShutdown();
	/* Chevron icons are shared, caller-owned textures (owned == 0 in the
	   buttons), so uiShutdown doesn't free them — delete them here. The moon
	   buttons reference Bodies_Moons() textures, which (as before) are not
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
		SimTime_TogglePause();
    }
    if (key == GLFW_KEY_T && action == GLFW_PRESS){
		wireframeMode = !wireframeMode;
    }
    // Sim-speed presets: 1 day/sec, 1 week/sec, 1 year/sec.
    if (key == GLFW_KEY_1 && action == GLFW_PRESS){
		SimTime_SetPreset(0);
    }
    if (key == GLFW_KEY_2 && action == GLFW_PRESS){
		SimTime_SetPreset(1);
    }
    if (key == GLFW_KEY_3 && action == GLFW_PRESS){
		SimTime_SetPreset(2);
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
			SimTime_TogglePause();
		}
		else if (id >= UI_ACT_SPEED_BASE && id < UI_ACT_SPEED_BASE + SimTime_PresetCount())
		{
			/* Sim-speed preset buttons are 300+index. */
			int si = id - UI_ACT_SPEED_BASE;
			SimTime_SetPreset(si);
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
	uiLayoutTextButtons(width, height);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	processMouseScroll((GLfloat)yoffset);
}