#ifndef CAMERA_H
#define CAMERA_H
#ifdef __APPLE__
# define __gl_h_
# define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#else
#include <glad/gl.h>
#endif
#include <GLFW/glfw3.h>

#include "LinearAlg.h"

enum Camera_Movement {
	FORWARD,
	BACKWARD,
	LEFT,
	RIGHT
};


extern float CameraDistance;
extern const float CameraDistanceMax;
extern GLfloat MovementSpeed;
extern GLfloat CameraScrollSpeed;

mat4 getViewMatrix();
mat4 getViewPosition();
mat4 getViewRotation();

vec3 getCameraPosition();
vec3 getCameraPivot();
void setCameraPivot(vec3 pivot);

void updateCameraVectors();
void processKeyboard(enum Camera_Movement direction, GLfloat deltaTime);
void processMousePress(GLFWwindow *window);
void processMouseRelease(void);
void processMouseMovement(GLFWwindow *window, GLfloat xpos, GLfloat ypos);
void processMouseScroll(GLfloat yoffset);

#endif