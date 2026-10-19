#ifndef BELT_H
#define BELT_H

#include "Common.h"

// GPU-instanced asteroid + Kuiper belts. Orbit positions are advanced on the
// CPU in double each frame (camera-relative) and refilled into a dynamic
// per-rock center/spin buffer, so the belts stay two instanced draws per frame
// with no float-precision orbit math in the shader.
typedef struct belt {
	char *meshPath;
	obj mesh;
	GLuint vao, vbo, ibo, cbo;
	int count;
} belt;

// Reseeds the shared scene LCG first (the belt is the first LCG consumer),
// then builds both belts from the shared layout stream.
void Belt_InitGL(void);
void Belt_SetSimTime(double days);
void Belt_Draw(const ViewMatrices *vm);
void Belt_Shutdown(void);

#endif
