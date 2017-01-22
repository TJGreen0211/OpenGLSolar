#version 410

in vec4 vPosition;
in vec3 vNormal;
in vec3 vTangent;

out vec3 fN;
out vec3 fE;
out vec3 fL;
out vec3 fH;

out vec4 TexCoords;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 translation;
uniform vec4 lightPos;
uniform vec3 camPosition;

void main()
{	
	gl_Position = vPosition*model*view*projection;
	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vec3 T = normalize(normalMatrix * vTangent);
	vec3 N = normalize(normalMatrix * vNormal);
	vec3 B = cross(N, T);
	
	vec3 lightDir = -normalize(vPosition*model - lightPos).xyz;
	vec3 vertexPosition = (vPosition*model).xyz;
	
	fL = vPosition.xyz;
	vec3 v;
	v.x = dot(lightDir, T);
	v.x = dot(lightDir, B);
	v.x = dot(lightDir, N);
	if( lightPos.w != 0.0 ) {
		fL = normalize(lightDir);//LightPosition.xyz - vPosition.xyz;
    }
	
	
    fN = vNormal*normalMatrix;
    
	v.x = dot(vertexPosition, T);
	v.x = dot(vertexPosition, B);
	v.x = dot(vertexPosition, N);
	fE = normalize(vertexPosition);
	
	vertexPosition = normalize(vertexPosition);
	
	vec3 halfVector = normalize(fL - fE);
	fH = halfVector;
	
    TexCoords = vPosition;
}