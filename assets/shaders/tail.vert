#version 410

// One quad per particle. aCorner is the per-vertex corner (divisor 0);
// aFracJit / aSizeAlpha are per-particle instance data (divisor 1).
layout(location=0) in vec2 aCorner;
layout(location=1) in vec3 aFracJit;   // x = position along tail 0..1, y/z = fixed jitter
layout(location=2) in vec2 aSizeAlpha; // x = sprite world diameter, y = relative alpha

uniform mat4 view;
uniform mat4 projection;
uniform vec3  uNucleus;    // nucleus world position
uniform vec3  uTailDir;    // unit anti-sunward direction
uniform float uTailLen;    // current tail length (proportional to 1/r^2)
uniform float uTailWidth;  // fan half-width at the tip (proportional to length)

out vec2  fUv;
out float fAlpha;

void main()
{
	float frac = aFracJit.x;

	// Particle centre: run out along the tail from the nucleus.
	vec3 center = uNucleus + uTailDir * (frac * uTailLen);

	// Fan-shaped spread in the plane around the tail axis, opening toward
	// the tip (width * frac -> 0 at the head).
	vec3 ref = (abs(uTailDir.y) < 0.9) ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
	vec3 side1 = normalize(cross(ref, uTailDir));
	vec3 side2 = cross(uTailDir, side1);
	center += (side1 * aFracJit.y + side2 * aFracJit.z) * (uTailWidth * frac);

	// Camera-facing billboard: the camera's world right/up come from the
	// rows of inverse(view), as a direction (w = 0 leaves out the translation).
	mat4 inv = inverse(view);
	vec3 camRight = vec3(vec4(1.0, 0.0, 0.0, 0.0) * inv);
	vec3 camUp    = vec3(vec4(0.0, 1.0, 0.0, 0.0) * inv);

	vec3 wp = center + camRight * (aCorner.x * aSizeAlpha.x)
                       + camUp    * (aCorner.y * aSizeAlpha.x);

	gl_Position = vec4(wp, 1.0) * view * projection;

	fUv = aCorner;                     // -0.5..0.5, centre at (0,0)
	fAlpha = aSizeAlpha.y * (1.0 - frac);  // fade toward the tip
}
