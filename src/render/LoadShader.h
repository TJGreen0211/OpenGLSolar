#ifndef LOADSHADER_H
#define LOADSHADER_H
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

GLuint LoadShader(char *shaderPath, int shaderType);

#endif