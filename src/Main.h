#ifndef MAIN_H
#define MAIN_H
#ifdef __APPLE__
# define __gl_h_
# define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>
//#include <GL/glew.h>
#include "Common.h"
#include "LinearAlg.h"
#include "Camera.h"
#include "Sphere.h"
#include "Geometry.h"
#include "Obj.h"
#include "Window.h"
#include "gui/GUI.h"
#include "SOIL/SOIL.h"
#include "gui/PlanetsUI.h"
#include "PlanetBuilder.h"



#define BUFFER_OFFSET( offset )   ((char*)NULL+(offset))

#endif