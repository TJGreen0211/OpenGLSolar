#include "Common.h"
#include "Main.h"
#include "Comet.h"

static comet comets[COMET_COUNT];
static GLuint cometShader, tailShader;

static void updateComet(comet *c, float dDays);

void Comet_InitGL(void)
{
	createShader(&cometShader, "assets/shaders/comet.vert",
		"assets/shaders/comet.frag");
	createShader(&tailShader, "assets/shaders/tail.vert",
		"assets/shaders/tail.frag");

	// Per-comet seeds. q (perihelion distance) sets how close each one dips to
	// the sun; rA (aphelion distance) sets how far out the ellipse arcs before
	// looping back. dir aims each perihelion into the camera-facing hemisphere
	// (world +z, ahead of the default camera at +5000) so the bright near-sun
	// pass happens in FRONT of the sun rather than behind it, from a
	// different
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
	c->mesh = ObjLoadModel("assets/obj/rock.obj");
	c->particleCount = COMET_PARTICLES;

	// Ellipse geometry, sun at the focus: e = (rA - q)/(rA + q), p = q(1+e).
	// r(theta) = p / (1 + e cos(theta)); theta spans the full -PI..+PI, with
	// perihelion at 0 and aphelion at +/-PI.
	c->q = seedQ[ci];
	c->e = (seedRA[ci] - seedQ[ci]) / (seedRA[ci] + seedQ[ci]);
	c->p = seedQ[ci] * (1.0f + c->e);
	c->dir = seedDir[ci];
	c->theta = seedPhase[ci] * 3.14159265358979f; // stagger the three comets
	// Speed in rad per simulated day: the old per-60-Hz-step constant
	// multiplied by OLD_TICKS_PER_DAY steps/day (see updateComet).
	c->orbitK = COMET_ORBIT_K * seedKMult[ci] * OLD_TICKS_PER_DAY;
	c->spin = SceneRand() * 6.2832f;

	// Nucleus VAO: positions then normals (same layout as initBelt).
	glGenVertexArrays(1, &c->vao);
	glBindVertexArray(c->vao);
	// Weld the loader's flat per-face normals into smooth per-vertex normals
	// before upload (same path as the belt and planet meshes).
	vec3 *nuc = malloc(sizeof(vec3) * c->mesh.vertexNumber);
	generateSmoothNormals(c->mesh.vertexNumber, nuc, c->mesh.points, c->mesh.normals);

	glGenBuffers(1, &c->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, c->vbo);
	glBufferData(GL_ARRAY_BUFFER, c->mesh.size + c->mesh.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, c->mesh.size, c->mesh.points);
	glBufferSubData(GL_ARRAY_BUFFER, c->mesh.size, c->mesh.nsize, nuc);
	free(nuc);
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
		float size = (24.0f + 150.0f * (1.0f - frac)) * (0.7f + 0.6f * SceneRand());
		inst[5*i + 0] = frac;
		inst[5*i + 1] = SceneRand() * 2.0f - 1.0f;
		inst[5*i + 2] = SceneRand() * 2.0f - 1.0f;
		inst[5*i + 3] = size;
		inst[5*i + 4] = 0.5f + 0.5f * SceneRand();
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
	// Seed the derived pose (r, pos, tail) at the chosen anomaly without
	// advancing: dDays=0 keeps theta exactly at seedPhase.
	updateComet(c, 0.0f);
	}
}

static void updateComet(comet *c, float dDays)
{
	// Ellipse, sun at the focus (origin): r = p / (1 + e cos theta), with
	// perihelion at theta = 0 and aphelion at theta = +/-PI. Advance theta by
	// (K / r^2) * dDays, where K (c->orbitK) is in rad per simulated day:
	// equal-area-in-equal-time, so it whips around perihelion and crawls far
	// out, like a real comet, at a pace tied to the sim clock. dDays is the
	// sim-days stepped this frame (updateBodies), so a pause steps it by 0.
	// The whole conic is rotated by c->dir so perihelion points along a chosen
	// world direction (kept near the default camera's line of sight).
	float ang = c->theta + c->dir;
	float r = c->p / (1.0f + c->e * cosf(c->theta));
	float dTheta = c->orbitK / (r * r);
	c->theta -= dTheta * dDays;
	if(c->theta < -3.14159265358979f)
		c->theta += 6.28318530717959f; // wrap at aphelion: same point, no visible reset
	c->spin += 0.005f * OLD_TICKS_PER_DAY * dDays;

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

void Comet_UpdateAll(float dDays)
{
	for(int ci = 0; ci < COMET_COUNT; ci++)
		updateComet(&comets[ci], dDays);
}

void Comet_Draw(const ViewMatrices *vm)
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
	glSetupMVP(cometShader, m, vm, vm->frame);
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
		glUniformMatrix4fv(glGetUniformLocation(tailShader, "projection"), 1, GL_FALSE, &vm->proj.m[0][0]);
		glUniformMatrix4fv(glGetUniformLocation(tailShader, "view"), 1, GL_FALSE, &vm->view.m[0][0]);
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

void Comet_Shutdown(void)
{
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
}
