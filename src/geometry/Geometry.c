#include "Geometry.h"

static int ringIdx;
static ring *ringOut;

static void ringVertex(vec3 p, float radius, float innerRadius, float outerRadius)
{
	vec3 up = {0.0f, 1.0f, 0.0f};
	float u = (radius - innerRadius) / (outerRadius - innerRadius);

	ringOut->points[ringIdx] = p;
	ringOut->normals[ringIdx] = up;
	ringOut->texCoords[ringIdx].x = u;
	ringOut->texCoords[ringIdx].y = 0.5f;
	ringIdx++;
}

ring createRing(int segments, float innerRadius, float outerRadius)
{
	ring r;
	int vertexNumber = segments * 2 * 3;

	r.size = vertexNumber * sizeof(vec3);
	r.nsize = vertexNumber * sizeof(vec3);
	r.texsize = vertexNumber * sizeof(vec2);
	r.points = malloc(r.size);
	r.normals = malloc(r.nsize);
	r.texCoords = malloc(r.texsize);

	ringIdx = 0;
	ringOut = &r;

	for(int i = 0; i < segments; i++)
	{
		float a0 = 2.0f * (float)M_PI * (float)i / (float)segments;
		float a1 = 2.0f * (float)M_PI * (float)(i + 1) / (float)segments;

		vec3 p00 = {innerRadius * cosf(a0), 0.0f, innerRadius * sinf(a0)};
		vec3 p01 = {outerRadius * cosf(a0), 0.0f, outerRadius * sinf(a0)};
		vec3 p10 = {innerRadius * cosf(a1), 0.0f, innerRadius * sinf(a1)};
		vec3 p11 = {outerRadius * cosf(a1), 0.0f, outerRadius * sinf(a1)};

		ringVertex(p00, innerRadius, innerRadius, outerRadius);
		ringVertex(p01, outerRadius, innerRadius, outerRadius);
		ringVertex(p10, innerRadius, innerRadius, outerRadius);

		ringVertex(p01, outerRadius, innerRadius, outerRadius);
		ringVertex(p11, outerRadius, innerRadius, outerRadius);
		ringVertex(p10, innerRadius, innerRadius, outerRadius);
	}

	r.vertexNumber = vertexNumber;
	return r;
}
