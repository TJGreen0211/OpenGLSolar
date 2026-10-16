#version 410

in vec3 fWorldPos;
in vec3 fNormal;

uniform float uFade;   // 0..1 pass-end fade window

out vec4 FragColor;

void main()
{
	vec3 N = normalize(fNormal);

	// Sun at the origin: lambert, a touch of ambient so the night side
	// still reads as a body.
	vec3 L = -normalize(fWorldPos);
	float light = 0.25 + 0.75 * max(dot(N, L), 0.0);

	// Icy nucleus: pale blue-white.
	vec3 base = vec3(0.72, 0.78, 0.86);

	FragColor = vec4(base * light, uFade);
}
