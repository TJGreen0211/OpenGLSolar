#version 410

in vec4 vPosition;
in vec3 vNormal;
in vec2 vTexture;

out vec3 fWorldPos;
out vec3 fNormal;
out vec2 fTexCoord;
out vec3 fCamPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	gl_Position = vPosition*model*view*projection;
	mat3 normalMatrix = transpose(inverse(mat3(model)));

	fWorldPos = (vPosition*model).xyz;
	fNormal = normalize(vec3(vec4(vNormal, 0.0))*normalMatrix);
	fTexCoord = vTexture;
	fCamPos = (vec4(0.0, 0.0, 0.0, 1.0)*inverse(view)).xyz;
}
