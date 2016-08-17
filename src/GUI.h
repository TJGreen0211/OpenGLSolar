#ifndef GUI_H
#define GUI_H
#ifdef __APPLE__
# define __gl_h_
# define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include "LinearAlg.h"
#include "../include/SOIL/src/SOIL.h"
#include "Common.h"

typedef struct imgButton {
	float xTopRight;
	float xTopLeft;
	float yTopRight;
	float yBottomRight;
	float *pointArray;
	int size;
	int state;
	int texture;
} imgButton;


imgButton initButton(float topx, float topy, float size, imgButton *button, char* texture);
void drawButton();
void buttonState(int state);
void attachGUIShaders();

#define BUFFER_OFFSET( offset )   ((char*)NULL+(offset))

#endif