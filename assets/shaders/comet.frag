#version 410

in vec3 fWorldPos;
in vec3 fNormal;
in float fViewDist;

uniform float uFade;   // 0..1 pass-end fade window
uniform vec3 uSunPos;  // camera-relative sun (universe origin)
uniform vec2 uLogDepth; // log2(near), log2(far)-log2(near): see Common.c

out vec4 FragColor;

void main()
{
	vec3 N = normalize(fNormal);

	// The sun lights from its camera-relative position (floating origin):
	// lambert, a touch of ambient so the night side still reads as a body.
	vec3 L = normalize(uSunPos - fWorldPos);
	float light = 0.25 + 0.75 * max(dot(N, L), 0.0);

	// Icy nucleus: pale blue-white.
	vec3 base = vec3(0.72, 0.78, 0.86);

	FragColor = vec4(base * light, uFade);
	gl_FragDepth = clamp((log2(max(fViewDist, 0.0001)) - uLogDepth.x) / uLogDepth.y,
	                     0.0, 1.0);
}
