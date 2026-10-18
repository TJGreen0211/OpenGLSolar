#include "Common.h"
#include "Main.h"
#include "Bodies.h"
#include "Planet.h"
#include "Moon.h"

void Moon_Draw(const ViewMatrices *vm)
{
	glUseProgram(Planet_Shader());
	glSetupPlanetLighting(Planet_Shader());
	// Moons have no shell of their own; clear the atmosphere flag the planet
	// pass left on the shared program so no extinction is applied, and clear
	// instanced mode so the plain tetrahedron mesh draws without the
	// quad-tree instance transform.
	glUniform1f(glGetUniformLocation(Planet_Shader(), "hasAtmosphere"), 0.0f);
	Planet_SetInstanced(0);
	int c = 0;
	for(int i = 0; i < 11; i++)
	{
		for(int j = 0; j < Bodies_Planets()[i].numMoons; j++)
		{
			vec3 translation;
			translation.x = (Bodies_Moons()[c].radius + Bodies_Planets()[i].size) * (float)cos(Bodies_MoonOrbitSpeed(c));
			translation.y = 0.0;
			translation.z = (Bodies_Moons()[c].radius + Bodies_Planets()[i].size) * (float)sin(Bodies_MoonOrbitSpeed(c));

			mat4 m = multiplymat4(multiplymat4(Planet_Transforms()[i], translatevec3(translation)), scale(Bodies_Moons()[c].size));

			glSetupMVP(Planet_Shader(), m, vm, vm->frame);

			glBindVertexArray (Planet_VAO());
			bindTexture(GL_TEXTURE0, Bodies_Moons()[c].texture);
			bindTexture(GL_TEXTURE1, Bodies_Moons()[c].normal);
			bindTexture(GL_TEXTURE2, Bodies_MoonDisplacementTexture());
			glUniform1i(glGetUniformLocation(Planet_Shader(), "tex"), 0);
			glUniform1i(glGetUniformLocation(Planet_Shader(), "normalTex"), 1);
			glUniform1i(glGetUniformLocation(Planet_Shader(), "depthMap"), 2);
			glDrawArrays( GL_TRIANGLES, 0, Planet_MeshVertexCount() );
			c++;
		}
	}
	glBindVertexArray(0);
}
