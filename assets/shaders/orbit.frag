#version 410

out vec4 FragColor;

uniform float uAlpha;

void main()
{
	FragColor = vec4(1.0, 1.0, 1.0, uAlpha);
}
