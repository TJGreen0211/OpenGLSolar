#ifndef CAMERA_H
#define CAMERA_H
#include <glad/gl.h>
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
mat4 getViewRotation();

/* Absolute (universe) camera/pivot position in double: never hand these to
   the GPU. Subtract them from world positions in double and downcast only
   the difference (see LinearAlg.h dvec3). */
dvec3 getCameraPosition();
dvec3 getCameraPivot();
void setCameraPivot(dvec3 pivot);

void updateCameraVectors();
void processKeyboard(enum Camera_Movement direction, GLfloat deltaTime);
void processMousePress(GLFWwindow *window);
void processMouseRelease(void);
void processMouseMovement(GLFWwindow *window, GLfloat xpos, GLfloat ypos);
void processMouseScroll(GLfloat yoffset);

#endif