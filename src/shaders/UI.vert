#version 410

in vec3 vPosition;
in vec2 texCoord;

uniform mat4 view;

out vec2 TexCoord;

void main()
{
	gl_Position = view*vec4(vPosition, 1.0);
	TexCoord = vec2(texCoord.x, 1.0 - texCoord.y);
}