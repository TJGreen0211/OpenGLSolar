#version 410

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec3 fWorldPos;
out vec3 fNormal;

void main()
{
	vec4 wp = vec4(aPosition, 1.0) * model;
	fWorldPos = wp.xyz;
	fNormal = mat3(model) * aNormal;

	// aNormal is already outward (loader centroid test + CPU smooth weld);
	// the old guard (see belt.vert) flipped the lit side away from the sun.
	// Row-vector chain, as in planet.vert.
	gl_Position = wp * view * projection;
}
