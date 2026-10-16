#include "Common.h"
#include "Main.h"
#include "Skybox.h"

static GLuint skyboxShader;
static GLuint skyboxVAO, skyboxVBO;

GLuint Skybox_LoadCube(void)
{
	char *cubemapArray[] = {
		"assets/textures/skybox3/right.png",
		"assets/textures/skybox3/left.png",
		"assets/textures/skybox3/top.png",
		"assets/textures/skybox3/bottom.png",
		"assets/textures/skybox3/back.png",
		"assets/textures/skybox3/front.png"
	};
	return loadCubemap(cubemapArray);
}

void Skybox_InitGL(void)
{
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

	createShader(&skyboxShader, "assets/shaders/skybox.vert",
		"assets/shaders/skybox.frag");

	glGenVertexArrays(1, &skyboxVAO);
	glBindVertexArray(skyboxVAO);
	glGenBuffers(1, &skyboxVBO);
	glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
	GLint position = glGetAttribLocation(skyboxShader, "vPosition");
	glEnableVertexAttribArray(position);
	glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
	glBindVertexArray(0);
}

void Skybox_Draw(GLuint cubemap, const ViewMatrices *vm)
{
	glUseProgram(skyboxShader);

	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);

	mat4 mv = getViewRotation();

	glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "projection"), 1, GL_TRUE, &vm->proj.m[0][0]);
	glUniformMatrix4fv(glGetUniformLocation(skyboxShader, "ModelView"), 1, GL_TRUE, &mv.m[0][0]);

	glBindVertexArray(skyboxVAO);
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubemap);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
}
