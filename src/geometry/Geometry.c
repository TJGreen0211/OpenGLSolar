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

vec3 *generateTangents(int vertexNumber, vec3 *points, vec3 *tangent)
{
	vec3 axis = {0.0, 0.0, 1.0};
	vec3 t;

	// The fragment shader samples the texture with the equirectangular
	// mapping u = atan2(y, x), v = asin(z), so the tangent is the
	// direction of increasing u: cross(axis, point) = (-y, x, 0).
	for(int i = 0; i < vertexNumber; i++)
	{
		t = crossvec3(axis, points[i]);
		if(t.x*t.x + t.y*t.y + t.z*t.z < 1e-8f)
		{
			// Exactly on a pole where the mapping degenerates.
			t.x = 1.0; t.y = 0.0; t.z = 0.0;
		}
		tangent[i] = normalizevec3(t);
	}
	return tangent;
}

vec3 *generateSmoothNormals(int vertexNumber, vec3 *vna, vec3 *pointArray, vec3 *normalArray)
{
	vec3 vn;

	for(int i = 0; i < vertexNumber; i++)
	{
		vec3 tempvn = {0.0, 0.0, 0.0};
		vn = pointArray[i];
		for(int j = 0; j < vertexNumber; j++)
		{
			if(vn.x == pointArray[j].x && vn.y == pointArray[j].y && vn.z == pointArray[j].z)
			{
				tempvn = plusequalvec3(tempvn, normalArray[j]);
			}
		}
		vna[i] = normalizevec3(tempvn);
	}
	return vna;
}
