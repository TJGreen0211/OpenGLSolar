#include "Common.h"
#include "Main.h"
#include "Sun.h"

// World-space radius of the sun's billboard (its on-screen size is this value
// projected at the sun's distance, so the sun shrinks as the camera pulls
// back). Tuned for the 1000x scene scale.
const float SUN_GLOW_RADIUS = 3000000.0f;

static GLuint sunTexture;
static GLuint sunNormal;
static GLuint sunShader;
static GLuint sunQuadVAO, sunQuadVBO;

void Sun_InitTextures(void (*step)(void))
{
	sunTexture = loadTexture("assets/textures/planets/sun2.jpg");
	step();
	sunNormal = loadTexture("assets/textures/planets/sunNormal.png");
	step();
}

void Sun_InitGL(void)
{
	createShader(&sunShader, "assets/shaders/sun.vert",
		"assets/shaders/sun.frag");

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

// Shared uniforms for both sun passes (same billboard, same projected centre
// and half-size). uDisc picks the fragment-shader mode: 1 = opaque disc,
// 0 = additive corona halo.
static void setupSunUniforms(const ViewMatrices *vm, GLfloat uDisc)
{
	glUniformMatrix4fv(glGetUniformLocation( sunShader, "projection" ), 1, GL_FALSE, &vm->proj.m[0][0]);
	glUniformMatrix4fv(glGetUniformLocation( sunShader, "view" ), 1, GL_FALSE, &vm->view.m[0][0]);
	glUploadLogDepth(sunShader);
	/* The sun sits at the universe origin: the shader projects it from its
	   camera-relative position (floating origin), not from the origin. */
	glUploadSunPos(sunShader);
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

void Sun_DrawDisc(const ViewMatrices *vm)
{
	glUseProgram(sunShader);
	setupSunUniforms(vm, 1.0f);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	drawSunQuadPass();
}

// Additive corona halo. Drawn LAST: depth-tested against the scene (so a
// passing planet still hides it) but never writes depth, so planets behind the
// halo stay visible and the glow simply blends over them.
void Sun_DrawGlow(const ViewMatrices *vm)
{
	glUseProgram(sunShader);
	setupSunUniforms(vm, 0.0f);

	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDepthFunc(GL_LEQUAL);
	glDepthMask(GL_FALSE);

	drawSunQuadPass();

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}
