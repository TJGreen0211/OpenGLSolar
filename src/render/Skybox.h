#ifndef SKYBOX_H
#define SKYBOX_H

#include "Common.h"

// Build and return the cubemap from the six skybox3 faces (the app runs this
// at its "[trace] main: loading skybox" milestone, before the first load
// budget is spent).
GLuint Skybox_LoadCube(void);

// Compile the skybox program and upload its 36-triangle box (the app's
// "[trace] init: skybox" milestone inside init()).
void Skybox_InitGL(void);

// Draw the box. Its ModelView is the camera's rotation-only matrix (no
// translation), so the box stays world-fixed around the viewer; its
// projection is vm->proj.
void Skybox_Draw(GLuint cubemap, const ViewMatrices *vm);

#endif
