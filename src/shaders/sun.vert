#version 410

in vec4 vPosition;
in vec3 vNormal;
in vec3 vTangent;
in vec3 vBitangent;

out vec3 fN;
out vec3 fE;
out vec3 fL;
out vec3 fH;

out vec4 TexCoords;

uniform mat4 view;
uniform mat4 model;
uniform mat4 projection;
uniform vec4 LightPosition;
uniform vec3 camPosition;

void main()
{
	mat4 ModelView = model*view;
	vec4 vLightPosition = ModelView*LightPosition*vec4(camPosition,1.0);
	gl_Position = vPosition*ModelView*projection;
	
	mat3 normalMatrix = transpose(inverse(mat3(ModelView)));
	vec3 T = normalize(normalMatrix * vTangent);
	vec3 N = normalize(normalMatrix * vNormal);
	vec3 B = cross(N, T);//normalize(normalMatrix * vBitangent);
	
	vec3 vertexPosition = vec3(vLightPosition).xyz;
	vec3 lightDir = normalize(vLightPosition.xyz - vPosition.xyz);
	
	
    fN = vNormal;
    fL = vec3(vPosition*ModelView).xyz;
    
    vec3 v;
	v.x = dot(lightDir, T);
	v.y = dot(lightDir, B);
	v.z = dot(lightDir, N);
	if( LightPosition.w != 0.0 ) {
		fL = normalize(lightDir);//LightPosition.xyz - vPosition.xyz;
    }
	
	v.x = dot(vertexPosition, T);
	v.y = dot(vertexPosition, B);
	v.z = dot(vertexPosition, N);
	fE = normalize(v);
	
	vertexPosition = normalize(vertexPosition);
	
	vec3 halfVector = normalize(fE + fL);
	v.x = dot (halfVector, T);
	v.y = dot (halfVector, B);
	v.z = dot (halfVector, N);
	fH = v;
    
    TexCoords = vPosition;
}

/*
	
	
	gl_Position = projection*ModelView*vPosition;
    fN = vNormal;
    fL = vec3(ModelView*vPosition).xyz;
    fE = vPosition.xyz;//(LightPosition.xyz-fL).xyz;
    if( LightPosition.w != 0.0 ) {
		fL = LightPosition.xyz - vPosition.xyz;
    }
    
    
    
    TexCoords = vPosition;

*/