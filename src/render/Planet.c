#include "Common.h"
#include "Main.h"
#include "Bodies.h"
#include "Planet.h"
#include "Camera.h"
#include "QuadTree.h"

static GLuint planetShader;
static GLuint planetVAO, planetVBO;
static int    planetVertexCount;
static GLint  planetInstancedLoc;
static GLint  planetDispAmpLoc = -1, planetDispLodLoc = -1;

// World-translation for each planet, written once per frame by Planet_Draw and
// read by the ring/moon/atmosphere passes (which still sit in Main.c until
// their own steps move them). Kept here because only the planet pass computes
// the centres.
static mat4   planetTransform[PLANET_COUNT];

void Planet_InitGL(sphere mesh)
{
	createShader(&planetShader, "assets/shaders/planet.vert",
		"assets/shaders/planet.frag");

	planetVertexCount = mesh.vertexNumber;

	vec3 *tangent = malloc(sizeof(vec3) * mesh.vertexNumber);
	generateTangents(mesh.vertexNumber, mesh.points, tangent);
	vec3 *vna = malloc(sizeof(vec3) * mesh.vertexNumber);
	generateSmoothNormals(mesh.vertexNumber, vna, mesh.points, mesh.normals);

	glGenVertexArrays(1, &planetVAO);
	glBindVertexArray(planetVAO);
	glGenBuffers(1, &planetVBO);
	glBindBuffer(GL_ARRAY_BUFFER, planetVBO);
	glBufferData(GL_ARRAY_BUFFER, mesh.size + mesh.nsize + sizeof(vec3) * mesh.vertexNumber, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0,	mesh.size, mesh.points);
	glBufferSubData(GL_ARRAY_BUFFER, mesh.size, mesh.nsize, vna);
	glBufferSubData(GL_ARRAY_BUFFER, mesh.size+mesh.nsize, sizeof(vec3) * mesh.vertexNumber,	tangent);

	GLuint vPosition = glGetAttribLocation(planetShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));

    GLuint vNormal = glGetAttribLocation(planetShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(mesh.size));

    GLuint vTangent = glGetAttribLocation(planetShader, "vTangent");
    glEnableVertexAttribArray(vTangent);
    glVertexAttribPointer(vTangent, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(mesh.size+mesh.nsize));

	glBindVertexArray(0);

	free(tangent);
	free(vna);

	// Quad-tree LOD sphere (ported terrain algorithm): six instanced face
	// grids replace the flat tetrahedron mesh for the planets themselves.
	planetInstancedLoc = glGetUniformLocation(planetShader, "instanced");
	planetDispAmpLoc = glGetUniformLocation(planetShader, "dispAmp");
	planetDispLodLoc = glGetUniformLocation(planetShader, "dispLodBase");
	QuadTree_InitGL(planetShader);
}

void Planet_Draw(const ViewMatrices *vm)
{
	glUseProgram(planetShader);
	glSetupPlanetLighting(planetShader);
	Planet_SetInstanced(1);
	for(int i = 0; i < PLANET_COUNT; i++)
	{
		vec3 translation;
		translation.x = Bodies_Planets()[i].radius * (float)cos(Bodies_OrbitSpeed(i));
		translation.y = 0.0;
		translation.z = Bodies_Planets()[i].radius * (float)sin(Bodies_OrbitSpeed(i));
		mat4 matT = multiplymat4(translatevec3(translation), scale(Bodies_Planets()[i].size));
		mat4 matR = multiplymat4(rotateY(Bodies_RotationSpeed(i)), rotateX(Bodies_Planets()[i].axialTilt+45.0f));
		mat4 m = multiplymat4(matT,matR);
		planetTransform[i] = translatevec3(translation);

		glSetupMVP(planetShader, m, vm, vm->frame);

    	bindTexture(GL_TEXTURE0, Bodies_Planets()[i].texture);
    	bindTexture(GL_TEXTURE1, Bodies_Planets()[i].normal);
    	bindTexture(GL_TEXTURE2, Bodies_Planets()[i].displacement);
    	glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
    	glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
		glUniform1i(glGetUniformLocation(planetShader, "depthMap"), 2);
    	glUniformMatrix4fv( glGetUniformLocation(planetShader, "translation" ), 1, GL_FALSE, &matT.m[0][0] );

    	// Atmospheric extinction for this body's own shell (matches the
    	// shell Atmosphere_Draw() renders: skipped when the scale factor
    	// leaves no real shell, e.g. Ceres). camLocalPos is the camera
    	// relative to the body centre, the space the shell spheres use.
    	int hasAtmo = Bodies_Planets()[i].createAtmosphere &&
    	              Bodies_Atmospheres()[i].scaleFactor > 1.0f;
    	vec3 camPos = getCameraPosition();
    	glUniform1f(glGetUniformLocation(planetShader, "hasAtmosphere"), hasAtmo ? 1.0f : 0.0f);
    	glUniform1f(glGetUniformLocation(planetShader, "fInnerRadius"), Bodies_Planets()[i].size);
    	glUniform1f(glGetUniformLocation(planetShader, "fOuterRadius"), Bodies_Planets()[i].size * Bodies_Atmospheres()[i].scaleFactor);
    	glUniform3f(glGetUniformLocation(planetShader, "C_R"), Bodies_Atmospheres()[i].C_R.x, Bodies_Atmospheres()[i].C_R.y, Bodies_Atmospheres()[i].C_R.z);
    	glUniform3f(glGetUniformLocation(planetShader, "camLocalPos"), camPos.x - translation.x, camPos.y - translation.y, camPos.z - translation.z);

    	// Height-derived-from-normal-map vertex displacement for this body.
    	glUniform1f(planetDispAmpLoc, Bodies_Planets()[i].dispScale);
    	glUniform1f(planetDispLodLoc, Bodies_Planets()[i].dispLodBase);

    	// Instanced cube-face quadtree sphere: one LOD walk + one instanced
    	// draw per face, subdivided around the camera (QuadTree.c).
    	QuadTree_DrawSphere(m, Bodies_Planets()[i].size);
    }
	Planet_SetInstanced(0);
	glBindVertexArray(0);
}

void Planet_SetInstanced(int on)
{
	glUniform1i(planetInstancedLoc, on);
}

GLuint Planet_Shader(void)          { return planetShader; }
GLuint Planet_VAO(void)             { return planetVAO; }
int    Planet_MeshVertexCount(void) { return planetVertexCount; }
const mat4 *Planet_Transforms(void) { return planetTransform; }

void Planet_Shutdown(void)
{
	QuadTree_Shutdown();
	glDeleteVertexArrays(1, &planetVAO);
	glDeleteBuffers(1, &planetVBO);
}
