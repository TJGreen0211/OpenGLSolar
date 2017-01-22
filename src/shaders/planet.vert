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
uniform vec4 lightPos;
uniform vec3 camPosition;
uniform mat4 lightMat4;

void main()
{	
	gl_Position = vPosition*model*view*projection;

	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vec4 LP = (lightPos-vPosition*model);
	vec3 T = normalize(normalMatrix * vTangent);
	vec3 N = normalize(normalMatrix * vNormal);
	vec3 B = cross(T, N);//normalize(normalMatrix * vBitangent);
	
	vec3 vertexPosition = (vPosition*model*view).xyz;
	vec3 lightDir = (LP-vPosition).xyz;
	
    fN = normalMatrix*vNormal;
    fL = vPosition.xyz;
    
    vec3 v;
	v.x = dot(lightDir, T);
	v.y = dot(lightDir, B);
	v.z = dot(lightDir, N);
	if( lightPos.w != 0.0 ) {
		fL = normalize(lightDir);//LightPosition.xyz - vPosition.xyz;
    }
	
	v.x = dot(vertexPosition, T);
	v.y = dot(vertexPosition, B);
	v.z = dot(vertexPosition, N);
	fE = normalize(vertexPosition);
	
	vertexPosition = normalize(vertexPosition);
	
	vec3 halfVector = normalize(LP.xyz + fL);
	v.x = dot (halfVector, T);
	v.y = dot (halfVector, B);
	v.z = dot (halfVector, N);
	fH = halfVector;
	
    TexCoords = vPosition;
}