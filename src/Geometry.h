#ifndef GEOMETRY_H
#define GEOMETRY_H

#include "LinearAlg.h"

typedef struct ring {
	int size;
	int nsize;
	int texsize;
	vec3 *points;
	vec3 *normals;
	vec2 *texCoords;
	int vertexNumber;
} ring;

// Flat annulus in the XZ plane (y = 0) around the origin. innerRadius and
// outerRadius are in the same units as the planet radius (1.0 = planet
// surface). Texture u runs 0 (inner edge) to 1 (outer edge); v is 0.5.
ring createRing(int segments, float innerRadius, float outerRadius);

#endif
