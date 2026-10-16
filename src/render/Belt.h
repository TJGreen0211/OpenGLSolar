#ifndef BELT_H
#define BELT_H

#include "Common.h"

// GPU-instanced asteroid + Kuiper belts. Each rock carries its whole orbit as
// instance data; the vertex shader advances it from uTime, so the belts cost
// two instanced draws per frame.
typedef struct belt {
	char *meshPath;
	obj mesh;
	GLuint vao, vbo, ibo;
	int count;
} belt;

// Reseeds the shared scene LCG first (the belt is the first LCG consumer),
// then builds both belts from the shared layout stream.
void Belt_InitGL(void);
void Belt_SetSimTime(float days);
void Belt_Draw(const ViewMatrices *vm);
void Belt_Shutdown(void);

#endif
