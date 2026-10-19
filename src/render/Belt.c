#include "Common.h"
#include "Main.h"
#include "Belt.h"
#include <math.h>

typedef struct beltRockData {
	float orbitRadius, height, angSpeed, phase;  // orbit (CPU-advanced)
	float scale, spinSpeed, spinPhase, tilt;     // aRock
} beltRockData;

static belt asteroidBelt;
static belt kuiperBelt;
static GLuint beltShader;
static double beltTime = 0.0;

/* Per-frame CPU orbit advance. The rock centres and spin angles are computed
   in double from simDays (no uTime float to overflow at large sim dates),
   the camera position is subtracted in double (floating origin), and only
   the small differences are downcast into the dynamic instance buffer. */
static void updateRockCenters(belt *b, const beltRockData *data)
{
	dvec3 cam = getCameraPosition();
	float *rel = malloc(sizeof(float) * 4 * b->count);
	for(int i = 0; i < b->count; i++)
	{
		double angle = (double)data[i].phase + beltTime * (double)data[i].angSpeed;
		double r = (double)data[i].orbitRadius;
		rel[4*i + 0] = (float)(r * cos(angle) - cam.x);
		rel[4*i + 1] = (float)((double)data[i].height - cam.y);
		rel[4*i + 2] = (float)(r * sin(angle) - cam.z);
		double spin = (double)data[i].spinPhase + beltTime * (double)data[i].spinSpeed;
		rel[4*i + 3] = (float)(spin - 6.283185307179586 * floor(spin / 6.283185307179586));
	}
	glBindBuffer(GL_ARRAY_BUFFER, b->cbo);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 4 * b->count, rel);
	free(rel);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Static generation parameters, kept so Belt_Draw can advance the orbits on
// the CPU each frame.
static beltRockData asteroidRockData[3000];
static beltRockData kuiperRockData[3000];

static void initBelt(belt *b, beltRockData *data, char *path, int count,
	float rMin, float rMax, float maxHeight, float sMin, float sMax)
{
	b->meshPath = path;
	b->count = count;
	b->mesh = ObjLoadModel(path);

	for(int i = 0; i < count; i++)
	{
		// Orbit radius: uniform across the belt's annulus.
		data[i].orbitRadius = rMin + SceneRand() * (rMax - rMin);
		// Small random height above/below the ecliptic.
		data[i].height = (SceneRand() * 2.0f - 1.0f) * maxHeight;
		// Angular speed in rad per simulated day. The old per-60-Hz-step value
		// (0.002f/orbitRadius at the pre-scale units, i.e. 0.001f/period with
		// period == radius/AU) times OLD_TICKS_PER_DAY steps/day, with +/-15%
		// jitter so the belt doesn't rotate as one solid ring. The numerator
		// carries the 1000x scene scale so the visual rotation pace is
		// unchanged at the larger radii.
		data[i].angSpeed = 2.0f / data[i].orbitRadius * (0.85f + 0.30f * SceneRand())
			* OLD_TICKS_PER_DAY;
		data[i].phase = SceneRand() * 6.2831853f;
		data[i].scale = sMin + SceneRand() * (sMax - sMin);
		// Self-spin, rad per simulated day (old per-frame step x steps/day).
		data[i].spinSpeed = (0.5f + SceneRand()) * 0.01f * OLD_TICKS_PER_DAY;
		if(SceneRand() < 0.5f)
			data[i].spinSpeed = -data[i].spinSpeed;
		data[i].spinPhase = SceneRand() * 6.2831853f;
		data[i].tilt = (SceneRand() * 2.0f - 1.0f) * 0.5f;
	}

	glGenVertexArrays(1, &b->vao);
	glBindVertexArray(b->vao);

	// Mesh stream: positions then normals. The
	// loader emits flat per-face normals (one per vertex of each triangle);
	// weld them to smooth per-vertex normals exactly like planet rendering
	// so the rocks shade smoothly instead of as a stack of facets.
	vec3 *smooth = malloc(sizeof(vec3) * b->mesh.vertexNumber);
	generateSmoothNormals(b->mesh.vertexNumber, smooth, b->mesh.points, b->mesh.normals);

	glGenBuffers(1, &b->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
	glBufferData(GL_ARRAY_BUFFER, b->mesh.size + b->mesh.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, b->mesh.size, b->mesh.points);
	glBufferSubData(GL_ARRAY_BUFFER, b->mesh.size, b->mesh.nsize, smooth);
	free(smooth);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(b->mesh.size));

	// Per-rock center/spin stream, refilled on the CPU each frame
	// (updateRockCenters): aCenterSpin = center (camera-relative) + spin angle.
	glGenBuffers(1, &b->cbo);
	glBindBuffer(GL_ARRAY_BUFFER, b->cbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * count, NULL, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 4*sizeof(GLfloat), BUFFER_OFFSET(0));
	glVertexAttribDivisor(2, 1);

	// Instance stream: one beltRockData per rock; the shader reads scale and
	// tilt out of the aRock half.
	glGenBuffers(1, &b->ibo);
	glBindBuffer(GL_ARRAY_BUFFER, b->ibo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(beltRockData) * count, data, GL_STATIC_DRAW);
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(beltRockData), BUFFER_OFFSET(4*sizeof(GLfloat)));
	glVertexAttribDivisor(3, 1);

	glBindVertexArray(0);
}

void Belt_InitGL(void)
{
	createShader(&beltShader, "assets/shaders/belt.vert",
		"assets/shaders/belt.frag");

	SceneReseed(0x5EEDB71u);

	// Main asteroid belt: between drawn Mars (1.88 * 2e6 = 3.76e6) and
	// Jupiter (11.86 * 2e6 = 2.37e7). All extents are in the 1000x scale.
	initBelt(&asteroidBelt, asteroidRockData, "assets/obj/asteroid.obj", 1500,
		4200000.0f, 6800000.0f, 120000.0f, 6000.0f, 30000.0f);
	// Kuiper belt: past drawn Jupiter. The far plane is dynamic
	// (updateDynamicProjection in Main.c bounds the whole system from the
	// camera), so it is visible from anywhere in the scene now.
	initBelt(&kuiperBelt, kuiperRockData, "assets/obj/rock.obj", 1000,
		35000000.0f, 95000000.0f, 1200000.0f, 15000.0f, 80000.0f);
}

void Belt_SetSimTime(double days)
{
	beltTime = days;
}

void Belt_Draw(const ViewMatrices *vm)
{
	glUseProgram(beltShader);
	glUniformMatrix4fv(glGetUniformLocation(beltShader, "projection"), 1, GL_FALSE, &vm->proj.m[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(beltShader, "view"), 1, GL_FALSE, &vm->view.m[0][0]);
	glUploadLogDepth(beltShader);
	// Sun at the universe origin: camera-relative position for belt.frag.
	glUploadSunPos(beltShader);

	belt *belts[2] = { &asteroidBelt, &kuiperBelt };
	beltRockData *datas[2] = { asteroidRockData, kuiperRockData };
	for(int i = 0; i < 2; i++)
	{
		updateRockCenters(belts[i], datas[i]);
		glBindVertexArray(belts[i]->vao);
		glDrawArraysInstanced(GL_TRIANGLES, 0, belts[i]->mesh.vertexNumber, belts[i]->count);
		glBindVertexArray(0);
	}
}

void Belt_Shutdown(void)
{
	glDeleteVertexArrays(1, &asteroidBelt.vao);
	glDeleteBuffers(1, &asteroidBelt.vbo);
	glDeleteBuffers(1, &asteroidBelt.ibo);
	glDeleteBuffers(1, &asteroidBelt.cbo);
	glDeleteVertexArrays(1, &kuiperBelt.vao);
	glDeleteBuffers(1, &kuiperBelt.vbo);
	glDeleteBuffers(1, &kuiperBelt.ibo);
	glDeleteBuffers(1, &kuiperBelt.cbo);
}
