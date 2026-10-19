#version 410

in vec3 fWorldPos;
in vec3 fNormal;
in float fShade;
in float fViewDist;

out vec4 FragColor;

uniform vec3 uSunPos;   // camera-relative sun (universe origin)
uniform vec2 uLogDepth; // log2(near), log2(far)-log2(near): see Common.c

void main()
{
	vec3 N = normalize(fNormal);

	// The sun lights from its camera-relative position (floating origin):
	// pure lambert, no specular on dust-sized rocks.
	vec3 L = normalize(uSunPos - fWorldPos);
	float light = 0.15 + 0.85 * max(dot(N, L), 0.0);

	// Slight per-rock albedo variation so a field of rocks reads as a
	// varied surface instead of one smeary grey.
	vec3 base = mix(vec3(0.40, 0.38, 0.36), vec3(0.58, 0.55, 0.50), fShade);

	FragColor = vec4(base * light, 1.0);
	gl_FragDepth = clamp((log2(max(fViewDist, 0.0001)) - uLogDepth.x) / uLogDepth.y,
	                     0.0, 1.0);
}
