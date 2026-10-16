#version 410

// Attribute is 3 floats; w pads to 1.0, and vec3*mat4 is not legal GLSL,
// so every shader here takes vec4 (same as planet.vert, ring.vert).
in vec4 vPosition;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = vPosition*model*view*projection;
}
