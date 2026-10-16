#version 410

in vec3 fWorldPos;
in vec3 fNormal;
in float fShade;

out vec4 FragColor;

void main()
{
	vec3 N = normalize(fNormal);

	// The sun sits at the origin of the system: pure lambert, no specular
	// on dust-sized rocks.
	vec3 L = -normalize(fWorldPos);
	float light = 0.15 + 0.85 * max(dot(N, L), 0.0);

	// Slight per-rock albedo variation so a field of rocks reads as a
	// varied surface instead of one smeary grey.
	vec3 base = mix(vec3(0.40, 0.38, 0.36), vec3(0.58, 0.55, 0.50), fShade);

	FragColor = vec4(base * light, 1.0);
}
