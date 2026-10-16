#include "Common.h"
#include "Main.h"
#include "Bodies.h"
#include "Orbits.h"

// Vertices per orbit circle (Orbits_Draw draws a GL_LINE_LOOP of this size).
#define ORBIT_SEGMENTS 256

static GLuint orbitShader, orbitVAO, orbitVBO;

void Orbits_InitGL(void)
{
	createShader(&orbitShader, "assets/shaders/orbit.vert",
		"assets/shaders/orbit.frag");

	// Unit circle in the XZ plane (the plane every planet orbits in). All
	// planets share it; Orbits_Draw scales it to each planet's orbit radius.
	GLfloat verts[ORBIT_SEGMENTS*3];
	for(int i = 0; i < ORBIT_SEGMENTS; i++)
	{
		float a = 2.0f*3.14159265358979f*(float)i/(float)ORBIT_SEGMENTS;
		verts[i*3+0] = cosf(a);
		verts[i*3+1] = 0.0f;
		verts[i*3+2] = sinf(a);
	}

	glGenVertexArrays(1, &orbitVAO);
	glBindVertexArray(orbitVAO);
	glGenBuffers(1, &orbitVBO);
	glBindBuffer(GL_ARRAY_BUFFER, orbitVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);

	GLint aPosition = glGetAttribLocation(orbitShader, "vPosition");
	glEnableVertexAttribArray(aPosition);
	glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), (GLvoid*)0);
	glBindVertexArray(0);
}

void Orbits_Draw(const ViewMatrices *vm)
{
	glUseProgram(orbitShader);

	// Faint white lines, the standard astronomy-app look. Depth is tested so
	// planet bodies hide the far side of an orbit, but the lines never write
	// depth, so they don't occlude anything themselves.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask(GL_FALSE);

	glUniform1f(glGetUniformLocation(orbitShader, "uAlpha"), 0.4f);

	for(int i = 0; i < PLANET_COUNT; i++)
	{
		// Same circle drawPlanet() orbits on: XZ plane, planet's radius as scale.
		mat4 m = scale(Bodies_Planets()[i].radius);
		glSetupMVP(orbitShader, m, vm, vm->frame);

		glBindVertexArray(orbitVAO);
		glDrawArrays(GL_LINE_LOOP, 0, ORBIT_SEGMENTS);
		glBindVertexArray(0);
	}

	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
}
