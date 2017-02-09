#ifndef COMMON_H
#define COMMON_H
#ifdef __APPLE__
# define __gl_h_
# define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
#include "../include/SOIL/src/SOIL.h"

void createShader(GLuint *shader, char *vert, char *frag);
GLuint loadCubemap(char **faces);
GLuint loadTexture(char const * path);
GLuint loadSpriteTexture(char const * path);
//GLfloat frameCount();

#endif