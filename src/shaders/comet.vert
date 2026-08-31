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

	// Same guard as belt.vert: the rock is convex and origin-centred, so an
	// outward normal always points away from the local origin.
	if(dot(fNormal, aPosition) < 0.0)
		fNormal = -fNormal;

	// Row-vector chain, as in planet.vert.
	gl_Position = wp * view * projection;
}
