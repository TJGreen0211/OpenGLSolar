#version 410

in vec4 color;
in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D newTexture1;

void main()
{
	FragColor = texture(newTexture1, TexCoord);
}
