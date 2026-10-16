#include "Common.h"
#include "Main.h"
#include "Bodies.h"
#include "Planet.h"
#include "Camera.h"
#include "Atmosphere.h"

static GLuint atmosphereShader;
static GLuint atmosphereVAO, atmosphereVBO;

void Atmosphere_InitGL(sphere mesh)
{
	createShader(&atmosphereShader, "assets/shaders/atmosphere.vert",
		"assets/shaders/atmosphere.frag");

    glGenVertexArrays(1, &atmosphereVAO);
	glBindVertexArray(atmosphereVAO);
	glGenBuffers(1, &atmosphereVBO);
	glBindBuffer(GL_ARRAY_BUFFER, atmosphereVBO);
	glBufferData(GL_ARRAY_BUFFER, mesh.size + mesh.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, mesh.size, mesh.points);
	glBufferSubData(GL_ARRAY_BUFFER, mesh.size, mesh.nsize, mesh.normals);
	GLuint vPosition = glGetAttribLocation(atmosphereShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));

    GLuint vNormal = glGetAttribLocation(atmosphereShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(mesh.size));
	glBindVertexArray(0);
}

void Atmosphere_Draw(const ViewMatrices *vm)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);

	glUseProgram(atmosphereShader);
	//glSetupPlanetLighting(atmosphereShader);


	for(int i = 0; i < 11; i++)
    {
     	if(Bodies_Planets()[i].createAtmosphere) {
			float scaleFactor = Bodies_Atmospheres()[i].scaleFactor;
			float fOuter = Bodies_Planets()[i].size*scaleFactor;
			float fInner = (Bodies_Planets()[i].size);
			if (fOuter <= fInner) {
				// no real shell (e.g. atmosphere scale not set); skip
				continue;
			}
			//translation.x = Bodies_Planets()[2].radius * cos(orbitSpeedArray[2]);
			//translation.y = 0.0;
			//translation.z = Bodies_Planets()[2].radius * sin(orbitSpeedArray[2]);

			mat4 modelmat = Planet_Transforms()[i];
			mat4 tv = transposemat4(multiplymat4(modelmat, getViewPosition()));
			vec4 camMult = {-tv.m[3][0], -tv.m[3][1], -tv.m[3][2], -tv.m[3][3]};
			vec4 camPosition = multiplymat4vec4(tv, camMult);

			//mat4 m = multiplymat4(translatevec3(translation), scale());
			mat4 m = multiplymat4(modelmat, scale(Bodies_Planets()[i].size*scaleFactor));
			// Per-body solar brightness + Rayleigh tint (set in atmosphereBuilder()).
			// The white Mie term is summed on top, so C_R's RGB sets each limb's hue.
			vec3 C_R = Bodies_Atmospheres()[i].C_R;
			float E = Bodies_Atmospheres()[i].E;

			glUniform1f(glGetUniformLocation(atmosphereShader, "fInnerRadius"), fInner);
			glUniform1f(glGetUniformLocation(atmosphereShader, "fOuterRadius"), fOuter);
			glUniform3f(glGetUniformLocation(atmosphereShader, "camPosition"), camPosition.x, camPosition.y, camPosition.z);
			glUniform3f(glGetUniformLocation(atmosphereShader, "C_R"), C_R.x, C_R.y, C_R.z);
			glUniform1f(glGetUniformLocation(atmosphereShader, "E"), E);
			glUniform1f(glGetUniformLocation(atmosphereShader, "time"), (float)glfwGetTime());

			glSetupMVP(atmosphereShader, m, vm, vm->frame);

			glBindVertexArray (atmosphereVAO);
			// The dome is an outward-facing sphere. When the camera is inside the
			// outer atmosphere every visible dome triangle is a back face, so
			// swap the cull face (the paper inverts the sky dome's front faces
			// for the camera-in-atmosphere case). The sky is a pure additive
			// pass and must not write depth, or its surface would hide the
			// moon/objects beyond the shell behind it.
			float camDistSq = camPosition.x*camPosition.x + camPosition.y*camPosition.y + camPosition.z*camPosition.z;
			glDepthMask(GL_FALSE);
			if (camDistSq < fOuter*fOuter) {
				glCullFace(GL_FRONT);
			}
			glDrawArrays( GL_TRIANGLES, 0, Planet_MeshVertexCount());
			glCullFace(GL_BACK);
			glDepthMask(GL_TRUE);
			glBindVertexArray(0);
		}
    }
    glDisable(GL_BLEND);
}
