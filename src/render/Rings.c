#include "Common.h"
#include "Main.h"
#include "Bodies.h"
#include "Planet.h"
#include "Geometry.h"
#include "Rings.h"

// ---------------------------------------------------------------------------
// Ring textures
//
// The old commit loaded assets/textures/planets/saturnRing.png, which was
// never committed. Synthesize an equivalent radial band profile instead: a
// 512x2 RGBA strip whose RGB is the ring colour and whose A is the
// per-radius brightness. u = 0 is the inner ring edge, u = 1 the outer one.
// ---------------------------------------------------------------------------

#define RING_TEX_WIDTH 512

static float ringGaussian(float u, float c, float w)
{
	float d = (u - c)/w;
	return expf(-0.5f*d*d);
}

// Saturn: D (faint inner), C, bright B, dark Cassini division, A, thin F
static float saturnRingLuma(float u)
{
	float l;
	if(u < 0.03f)      l = 0.10f;
	else if(u < 0.20f) l = 0.28f;
	else if(u < 0.58f) l = 0.85f;
	else if(u < 0.66f) l = 0.14f;
	else               l = 0.55f;

	if(u > 0.895f && u < 0.915f)
		l *= 0.25f;
	if(u >= 0.985f)
		l = 0.35f;

	l *= 0.93f + 0.07f*sinf(u*300.0f + 1.7f)*sinf(u*77.0f);
	return l;
}

// Uranus: very faint narrow rings, brightest = epsilon ring near u ~ 0.9
static float uranusRingLuma(float u)
{
	float l = 0.03f;
	l += 0.10f*ringGaussian(u, 0.30f, 0.015f);
	l += 0.14f*ringGaussian(u, 0.45f, 0.020f);
	l += 0.12f*ringGaussian(u, 0.62f, 0.012f);
	l += 0.55f*ringGaussian(u, 0.90f, 0.012f);
	if(l > 1.0f)
		l = 1.0f;
	return l;
}

static GLuint uploadRingTexture(float baseR, float baseG, float baseB, float (*luma)(float))
{
	unsigned char pixels[RING_TEX_WIDTH*2*4];

	for(int x = 0; x < RING_TEX_WIDTH; x++)
	{
		float l = luma((float)x/(float)(RING_TEX_WIDTH - 1));
		for(int y = 0; y < 2; y++)
		{
			unsigned char *px = &pixels[(x*2 + y)*4];
			px[0] = (unsigned char)(baseR*255.0f);
			px[1] = (unsigned char)(baseG*255.0f);
			px[2] = (unsigned char)(baseB*255.0f);
			px[3] = (unsigned char)(l*255.0f);
		}
	}

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, RING_TEX_WIDTH, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
	return textureID;
}

// Bodies_Planets() order from planetBuilder(): 6 = saturn, 7 = uranus
static GLuint generateRingTexture(int planetIndex)
{
	if(planetIndex == 6)
		return uploadRingTexture(0.93f, 0.85f, 0.70f, saturnRingLuma);
	return uploadRingTexture(0.65f, 0.75f, 0.88f, uranusRingLuma);
}

static GLuint ringShader;
static GLuint ringVAO[PLANET_COUNT], ringVBO[PLANET_COUNT];
static ring   ringArray[PLANET_COUNT];
static GLuint ringTex[PLANET_COUNT];

void Rings_InitGL(void)
{
	createShader(&ringShader, "assets/shaders/ring.vert",
		"assets/shaders/ring.frag");

	for(int i = 0; i < PLANET_COUNT; i++)
	{
		if(!Bodies_Planets()[i].createRing)
			continue;

		ringTex[i] = generateRingTexture(i);
		ringArray[i] = createRing(128, Bodies_Planets()[i].ringInner, Bodies_Planets()[i].ringOuter);

		glGenVertexArrays(1, &ringVAO[i]);
		glBindVertexArray(ringVAO[i]);
		glGenBuffers(1, &ringVBO[i]);
		glBindBuffer(GL_ARRAY_BUFFER, ringVBO[i]);
		glBufferData(GL_ARRAY_BUFFER, ringArray[i].size + ringArray[i].nsize + ringArray[i].texsize, NULL, GL_STATIC_DRAW);
		glBufferSubData(GL_ARRAY_BUFFER, 0, ringArray[i].size, ringArray[i].points);
		glBufferSubData(GL_ARRAY_BUFFER, ringArray[i].size, ringArray[i].nsize, ringArray[i].normals);
		glBufferSubData(GL_ARRAY_BUFFER, ringArray[i].size + ringArray[i].nsize, ringArray[i].texsize, ringArray[i].texCoords);

		GLint aPosition = glGetAttribLocation(ringShader, "vPosition");
		glEnableVertexAttribArray(aPosition);
		glVertexAttribPointer(aPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));

		GLint aNormal = glGetAttribLocation(ringShader, "vNormal");
		glEnableVertexAttribArray(aNormal);
		glVertexAttribPointer(aNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(ringArray[i].size));

		GLint aTexture = glGetAttribLocation(ringShader, "vTexture");
		glEnableVertexAttribArray(aTexture);
		glVertexAttribPointer(aTexture, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), BUFFER_OFFSET(ringArray[i].size + ringArray[i].nsize));
		glBindVertexArray(0);
	}
}

void Rings_Draw(const ViewMatrices *vm)
{
	glUseProgram(ringShader);

	for(int i = 0; i < PLANET_COUNT; i++)
	{
		if(Bodies_Planets()[i].createRing) {
			// Additive glow like the atmosphere, but alpha-weighted so the band
			// gaps (e.g. the Cassini division) show the planet through. Depth
			// is tested but not written: the planet drawn earlier already hides
			// the far ring, and the ring is a pure overlay in front of it.
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			glDisable(GL_CULL_FACE);
			glDepthMask(GL_FALSE);

			mat4 modelmat = Planet_Transforms()[i];
			mat4 m = multiplymat4(multiplymat4(modelmat, rotateX(Bodies_Planets()[i].axialTilt+45.0f)),
				scale(Bodies_Planets()[i].size));

			glSetupMVP(ringShader, m, vm, vm->frame);
			glUniform3f(glGetUniformLocation(ringShader, "planetCenter"),
				modelmat.m[0][3], modelmat.m[1][3], modelmat.m[2][3]);
			glUniform1f(glGetUniformLocation(ringShader, "planetRadius"), Bodies_Planets()[i].size);

			glBindVertexArray(ringVAO[i]);
			bindTexture(GL_TEXTURE0, ringTex[i]);
			glUniform1i(glGetUniformLocation(ringShader, "tex"), 0);
			glDrawArrays( GL_TRIANGLES, 0, ringArray[i].vertexNumber);
			glBindVertexArray(0);

			glEnable(GL_CULL_FACE);
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE);
		}
	}
}

void Rings_Shutdown(void)
{
	for(int i = 0; i < PLANET_COUNT; i++)
	{
		if(Bodies_Planets()[i].createRing)
		{
			glDeleteVertexArrays(1, &ringVAO[i]);
			glDeleteBuffers(1, &ringVBO[i]);
			free(ringArray[i].points);
			free(ringArray[i].normals);
			free(ringArray[i].texCoords);
			glDeleteTextures(1, &ringTex[i]);
		}
	}
}
