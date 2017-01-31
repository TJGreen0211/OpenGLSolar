#version 410

in vec4 vPosition;
in vec3 vNormal;
in vec3 vTangent;

out vec3 fN;
out vec3 fE;
out vec3 fL;
out vec3 fH;
out vec3 fV;

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
	vec3 T = normalize(vec3(vec4(vTangent, 0.0) * model));
	vec3 N = normalize(vec3(vec4(vNormal, 0.0)) * normalMatrix);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T);
	
	vec3 lightDir = -normalize(vPosition*model - lightPos).xyz;
	vec3 vertexPosition = (vPosition*model).xyz;
	
	fL = vPosition.xyz;
	vec3 v;
	v.x = dot(lightDir, T);
	v.y = dot(lightDir, B);
	v.z = dot(lightDir, N);
	if( lightPos.w != 0.0 ) {
		fL = normalize(v);//LightPosition.xyz - vPosition.xyz;
    }
	
    fN = vNormal*normalMatrix;
    
	v.x = dot(vertexPosition, T);
	v.y = dot(vertexPosition, B);
	v.z = dot(vertexPosition, N);
	fE = normalize(v);
	
	vec3 ray = vec3(model*vPosition) - camPosition;
	v.x = dot(ray, T);
	v.y = dot(ray, B);
	v.z = dot(ray, N);
	fV = normalize(vertexPosition);
	
	vertexPosition = normalize(vertexPosition);
	
	
	vec3 halfVector = normalize(lightDir - vertexPosition);
	v.x = dot(halfVector, T);
	v.y = dot(halfVector, B);
	v.z = dot(halfVector, N);
	fH = v;
	
    TexCoords = vPosition;
}