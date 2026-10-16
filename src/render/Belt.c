#include "Common.h"
#include "Main.h"
#include "Belt.h"

typedef struct beltRockData {
	float orbitRadius, height, angSpeed, phase;  // aOrbit
	float scale, spinSpeed, spinPhase, tilt;     // aRock
} beltRockData;

static belt asteroidBelt;
static belt kuiperBelt;
static GLuint beltShader;
static float beltTime = 0.0f;

static void initBelt(belt *b, char *path, int count,
	float rMin, float rMax, float maxHeight, float sMin, float sMax)
{
	b->meshPath = path;
	b->count = count;
	b->mesh = ObjLoadModel(path);

	beltRockData *data = malloc(sizeof(beltRockData) * count);
	for(int i = 0; i < count; i++)
	{
		// Orbit radius: uniform across the belt's annulus.
		data[i].orbitRadius = rMin + SceneRand() * (rMax - rMin);
		// Small random height above/below the ecliptic.
		data[i].height = (SceneRand() * 2.0f - 1.0f) * maxHeight;
		// Angular speed in rad per simulated day. The old per-60-Hz-step value
		// (0.002f/orbitRadius, i.e. 0.001f/period with period == radius/AU)
		// times OLD_TICKS_PER_DAY steps/day, with +/-15% jitter so the belt
		// doesn't rotate as one solid ring. The belt shader advances
		// angle = aOrbit.w + uTime * aOrbit.z with uTime now in days.
		data[i].angSpeed = 0.002f / data[i].orbitRadius * (0.85f + 0.30f * SceneRand())
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

void Belt_InitGL(void)
{
	createShader(&beltShader, "assets/shaders/belt.vert",
		"assets/shaders/belt.frag");

	SceneReseed(0x5EEDB71u);

	// Main asteroid belt: between drawn Mars (1.88 * 2000 = 3760) and
	// Jupiter (11.86 * 2000 = 23724).
	initBelt(&asteroidBelt, "assets/obj/asteroid.obj", 1500,
		4200.0f, 6800.0f, 120.0f, 6.0f, 30.0f);
	// Kuiper belt: past drawn Jupiter, kept inside the view frustum
	// (zFar = 100000, camera distance max 25000) — the specced "past
	// Neptune" range (Neptune draws at 329600) is beyond the far plane
	// and can never be seen.
	initBelt(&kuiperBelt, "assets/obj/rock.obj", 1000,
		35000.0f, 95000.0f, 1200.0f, 15.0f, 80.0f);
}

void Belt_SetSimTime(float days)
{
	beltTime = days;
}

void Belt_Draw(const ViewMatrices *vm)
{
	glUseProgram(beltShader);
	glUniformMatrix4fv(glGetUniformLocation(beltShader, "projection"), 1, GL_FALSE, &vm->proj.m[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(beltShader, "view"), 1, GL_FALSE, &vm->view.m[0][0]);
	glUniform1f(glGetUniformLocation(beltShader, "uTime"), beltTime);

	belt *belts[2] = { &asteroidBelt, &kuiperBelt };
	for(int i = 0; i < 2; i++)
	{
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
	glDeleteVertexArrays(1, &kuiperBelt.vao);
	glDeleteBuffers(1, &kuiperBelt.vbo);
	glDeleteBuffers(1, &kuiperBelt.ibo);
}
