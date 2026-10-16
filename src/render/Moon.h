#ifndef MOON_H
#define MOON_H

#include "Common.h"

// Draw every moon using the planet program/VAO (Planet_Shader()/Planet_VAO()),
// placing each orbit around the centre of the planet it belongs to (via
// Planet_Transforms()); depth-tested with per-moon texture, normal and shared
// displacement maps.
void Moon_Draw(const ViewMatrices *vm);

#endif
