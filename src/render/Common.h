#ifndef COMMON_H
#define COMMON_H
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "SOIL.h"
#include "LinearAlg.h"

// Shared per-frame view-space matrices for every *_Draw / glSetupMVP. The App
// loop builds one each frame (proj from the fixed startup projection, view
// from the camera) and passes a pointer down to each render module.
typedef struct ViewMatrices {
	mat4 proj;
	mat4 view;
	// The app loop's per-frame counter, mirrored from its local gFrame so render
	// modules (which can't see that local) can key the glSetupMVP proj/view cache
	// and get one fresh upload per frame.
	int frame;
} ViewMatrices;

// Interleaved VBO attribute offset helper, shared render-layer-wide.
#define BUFFER_OFFSET( offset )   ((char*)NULL+(offset))

// Ticks (of the old 60-Hz reference frame) per simulated day. The old motion
// constants were tuned per 60-Hz frame at a fixed pace of 3.48898 sim-days/sec,
// so one old day = 60/3.48898 frames. Used to rebase the belt spin and comet
// speeds from per-frame to rad-per-day.
#define OLD_TICKS_PER_DAY 17.2023f

void createShader(GLuint *shader, char *vert, char *frag);
GLuint loadCubemap(char **faces);
GLuint loadTexture(char const * path);
//GLfloat frameCount();

// Per-frame model/projection/view upload: proj+view go up once per program per
// frame (cache in Common.c); model goes up on every call. proj/view are taken
// explicitly (const ViewMatrices*) instead of the app's global p/v.
void glSetupMVP(GLuint shader, mat4 model, const ViewMatrices *vm, int frame);
void bindTexture(GLuint activeTex, GLuint tex);
void glSetupPlanetLighting(GLuint program);
// The single scene light position (world space). The planet pass uploads it as
// lightPos and the atmosphere pass reads it so their terminators agree.
vec4 glGetLightPosition(void);
// Shared deterministic LCG for scene layout (belt + comet). Both must draw from
// the same instance so their layout stays bit-reproducible.
float SceneRand(void);
void SceneReseed(unsigned int seed);

#endif