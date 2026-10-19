#include "Common.h"
#include "Main.h"
#include "Bodies.h"
#include "Planet.h"
#include "Moon.h"

void Moon_Draw(const ViewMatrices *vm)
{
	glUseProgram(Planet_Shader());
	glSetupPlanetLighting(Planet_Shader());
	glUniform1f(glGetUniformLocation(Planet_Shader(), "waterSpec"), 0.0f);
	// clear instanced mode so the plain tetrahedron mesh draws without the
	// quad-tree instance transform.
	Planet_SetInstanced(0);
	dvec3 cam = getCameraPosition();
	int c = 0;
	for(int i = 0; i < 11; i++)
	{
		if (Bodies_Planets()[i].numMoons == 0) {
			continue;
		}
		// Moons have no shell of their own but sit in (or beyond) their
		// parent's: apply the parent body's atmosphere uniforms so a moon
		// inside the shell gets fogged like the planet surface, and one
		// beyond it still gets the shell chord's fog and in-scatter.
		// shellOffset carries each moon's offset from the parent centre so
		// planet.frag builds the extinction ray in the shell-centred frame.
		int hasAtmo = Bodies_Planets()[i].createAtmosphere &&
		              Bodies_Atmospheres()[i].scaleFactor > 1.0f;
		glUniform1f(glGetUniformLocation(Planet_Shader(), "hasAtmosphere"), hasAtmo ? 1.0f : 0.0f);
		glUniform1f(glGetUniformLocation(Planet_Shader(), "fInnerRadius"), Bodies_Planets()[i].size);
		glUniform1f(glGetUniformLocation(Planet_Shader(), "fOuterRadius"), Bodies_Planets()[i].size * Bodies_Atmospheres()[i].scaleFactor);
		glUniform3f(glGetUniformLocation(Planet_Shader(), "C_R"), Bodies_Atmospheres()[i].C_R.x, Bodies_Atmospheres()[i].C_R.y, Bodies_Atmospheres()[i].C_R.z);
		glUniform1f(glGetUniformLocation(Planet_Shader(), "E"), Bodies_Atmospheres()[i].E);
		// Camera relative to the parent centre: camera is the origin of the
		// camera-relative space, so it is just -planetTransform.
		const mat4 *parentT = &Planet_Transforms()[i];
		glUniform3f(glGetUniformLocation(Planet_Shader(), "camLocalPos"),
			-parentT->m[0][3], -parentT->m[1][3], -parentT->m[2][3]);
		for(int j = 0; j < Bodies_Planets()[i].numMoons; j++)
		{
			// Universe-accurate moon centre in double (parent centre plus
			// orbit offset), then camera-relative and downcast once.
			dvec3 moonWorld = Bodies_MoonWorldPosition(c);
			dvec3 rel = dsubvec3(moonWorld, cam);
			vec3 translation = dvec3tovec3(rel);
			dvec3 shellOff = dsubvec3(moonWorld, Bodies_PlanetWorldPosition(i));
			glUniform3f(glGetUniformLocation(Planet_Shader(), "shellOffset"),
				(float)shellOff.x, (float)shellOff.y, (float)shellOff.z);

			mat4 m = multiplymat4(translatevec3(translation), scale(Bodies_Moons()[c].size));

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
