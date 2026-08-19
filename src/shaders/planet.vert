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

void main()
{
	gl_Position = vPosition*model*view*projection;
	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vec3 T = normalize(vec3(vec4(vTangent, 0.0) * model));
	vec3 N = normalize(vec3(vec4(vNormal, 0.0)) * normalMatrix);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T);
	
	vec3 vertexPosition = (vPosition*model).xyz;
	vec3 camPosition = (vec4(0.0, 0.0, 0.0, 1.0) * inverse(view)).xyz;
	// camera -> fragment direction in world space
	vec3 viewDir = normalize(vertexPosition - camPosition);
	vec3 lightDir = normalize(lightPos.xyz - vertexPosition);
	
	fL = vPosition.xyz;
	vec3 v;
	v.x = dot(lightDir, T);
	v.y = dot(lightDir, B);
	v.z = dot(lightDir, N);
	if( lightPos.w != 0.0 ) {
		fL = normalize(v);
    }
	
	

    fN = vNormal*normalMatrix;
    
	v.x = dot(viewDir, T);
	v.y = dot(viewDir, B);
	v.z = dot(viewDir, N);
	fE = normalize(v);
	
	vec3 halfVector = normalize(lightDir - viewDir);
	v.x = dot(halfVector, T);
	v.y = dot(halfVector, B);
	v.z = dot(halfVector, N);
	fH = v;
	
    TexCoords = vPosition;
}