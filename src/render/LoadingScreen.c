#include "Common.h"
#include "LoadingScreen.h"
#include "Bodies.h"

static GLFWwindow *loadWindow = NULL;
static GLuint loadingShader, loadingVAO, loadingVBO;
static int loadStepDone = 0;

// Worst-case number of discrete load steps: skybox (1) + sun (2) +
// planet textures (11x3) + moons (neutral normal 1 + displacement 1 + up to
// MAX_MOONS textures) + atmosphere (1) + init (1) + UI buttons (13) +
// moon panels (up to 11 toggles + MAX_MOONS moon buttons) + UI shader (1).
// If some moon textures are missing the bar tops out below 100% and the
// final frame forces it to 1.0, which is fine.
#define LOAD_TOTAL_STEPS (1 + 2 + 33 + 2 + MAX_MOONS + 16 + 11 + MAX_MOONS)

void LoadingScreen_Init(GLFWwindow *window)
{
	loadWindow = window;

	createShader(&loadingShader, "assets/shaders/loading.vert",
		"assets/shaders/loading.frag");

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

	loadStepDone = 0;
}

void LoadingScreen_Render(float progress)
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
void LoadingScreen_Step(void)
{
	float pct = (float)loadStepDone / (float)LOAD_TOTAL_STEPS;
	loadStepDone++;
	if (pct > 1.0f)
		pct = 1.0f;
	LoadingScreen_Render(pct);
}

void LoadingScreen_Finish(void)
{
	LoadingScreen_Render(1.0f);
	loadWindow = NULL;
}
