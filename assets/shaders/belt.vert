#version 410

layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;

// Per-instance data (one stream per rock, divisor 1):
// aCenterSpin: xyz rock centre, camera-relative (the CPU advanced the orbit
//              in double and subtracted the camera — floating origin),
//              w self-spin angle in radians (CPU-advanced too).
// aRock:  x uniform scale,
//         y spin speed (unused here),
//         z spin phase (unused here),
//         w axial tilt (rad, about the rock's X axis)
layout(location=2) in vec4 aCenterSpin;
layout(location=3) in vec4 aRock;

uniform mat4 view;
uniform mat4 projection;

out vec3 fWorldPos;
out vec3 fNormal;
out float fShade;
out float fViewDist;   // view-space distance ahead of the camera

mat3 rotY(float a)
{
	float s = sin(a), c = cos(a);
	return mat3(c, 0.0, s, 0.0, 1.0, 0.0, -s, 0.0, c);
}

mat3 rotZ(float a)
{
	float s = sin(a), c = cos(a);
	return mat3(c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0);
}

void main()
{
	// Rock centre, already camera-relative (orbit advanced on the CPU in
	// double, so no float uTime precision loss at large sim dates).
	vec3 center = aCenterSpin.xyz;

	// Local spin plus a fixed per-rock tilt, then uniform scale.
	mat3 R = rotY(aCenterSpin.w) * rotZ(aRock.w);
	vec3 p = R * (aPosition * aRock.x);
	vec3 n = R * aNormal;

	// aNormal is already outward: the loader enforces it (centroid test) and
	// the CPU welds it to a smooth normal, so no per-vertex reorientation.
	// The old guard compared this rotated normal against the unrotated local
	// position (a frame mismatch) and, because the mesh is not origin-
	// centred, flipped correct normals as the rock spun — which is what lit
	// the side facing away from the sun.
	fWorldPos = p + center;
	fNormal = n;
	fShade = fract(sin(aRock.z * 12.9898) * 43758.5453);

	// Same row-vector chain as planet.vert: vPosition*model*view*projection.
	// The app's matrices are uploaded as &m[0][0] of a row-major mat4,
	// so column-vector chains (projection*view*p) land mirrored.
	gl_Position = vec4(fWorldPos, 1.0) * view * projection;
	fViewDist = gl_Position.w;
}
