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

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 LightPosition;
uniform vec3 camPosition;

uniform vec3 v3CameraPos;
uniform vec3 v3InvWavelength;

uniform float fCameraHeight;
uniform float fCameraHeight2;
uniform float fInnerRadius;
uniform float fInnerRadius2;
uniform float fOuterRadius;
uniform float fOuterRadius2;

uniform float fKrESun;
uniform float fKmESun;
uniform float fKr4PI;
uniform float fKm4PI;
uniform float fScale;
uniform float fScaleOverScaleDepth;

const int nSamples = 2;
const float fSamples = 2.0;

invariant out vec3 v3Direction;

out vec3 frontColor;
out vec3 frontSecondaryColor;


float scale(float fCos)
{
	float x = 1.0 - fCos;
	return fScale * exp(-0.00287 + x*(0.459 + x*(3.83 + x*(-6.80 + x*5.25))));
}


void main()
{	
	/*
	//fragPos = model*vPosition
	//viewPos = mvp
	
	vec3 v3LightPos = LightPosition.xyz;
	
	vec3 v3Pos = vPosition.xyz;
	vec3 v3Ray= v3Pos - v3CameraPos;
	float fFar = length(v3Ray);
	v3Ray /= fFar;
	
	vec3 v3Start = v3CameraPos;
	float fHeight = length(v3Start);
	float fDepth = exp(fScaleOverScaleDepth * (fInnerRadius - fCameraHeight));
	float fStartAngle = dot(v3Ray, v3Start) / fHeight;
	float fStartOffset = fDepth*scale(fStartAngle);
	
	frontColor = vec4(0.0, 0.0, 0.0, 0.0);
	frontSecondaryColor = vec4(0.0, 0.0, 0.0, 0.0);
	
	float fSampleLength = fFar / fSamples;
	float fScaledLength = fSampleLength * fScale;
	vec3 v3SampleRay = v3Ray * fSampleLength;
	vec3 v3SamplePoint = v3Start + v3SampleRay * 0.5;
	
	vec3 v3FrontColor = vec3(0.2, 0.1, 0.0);
	for(int i = 0; i < nSamples; i++)
	{
		float fHeight = length(v3SamplePoint);
    	float fDepth = exp(fScaleOverScaleDepth * (fInnerRadius - fHeight));
    	float fLightAngle = dot(v3LightPos, v3SamplePoint) / fHeight;
    	float fCameraAngle = dot(v3Ray, v3SamplePoint) / fHeight;
    	float fScatter = (fStartOffset + fDepth*(scale(fLightAngle) - scale(fCameraAngle)));
    	vec3 v3Attenuate = exp(-fScatter * (v3InvWavelength * fKr4PI + fKm4PI));
    	v3FrontColor += v3Attenuate * (fDepth * fScaledLength);
    	v3SamplePoint += v3SampleRay;
	}
	
	frontSecondaryColor.rgb = v3FrontColor * fKmESun;
	frontColor.rgb = v3FrontColor * (v3InvWavelength * fKrESun);
	v3Direction = v3CameraPos - v3Pos;
	
	secondaryColor = frontSecondaryColor;
	color = frontColor;
	*/
	
	mat4 ModelView = view*model;
	gl_Position = projection*ModelView*vPosition;

	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vec4 LP = (LightPosition-model*vPosition);
	vec3 T = normalize(normalMatrix * vTangent);
	vec3 N = normalize(normalMatrix * vNormal);
	vec3 B = cross(T, N);//normalize(normalMatrix * vBitangent);
	
	vec3 vertexPosition = (ModelView*vPosition).xyz*camPosition;
	vec3 lightDir = (LP-vPosition).xyz;
	
    fN = normalMatrix*vNormal;
    fL = lightDir;
    
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
	fE = normalize(vertexPosition);
	
	vertexPosition = normalize(vertexPosition);
	
	vec3 halfVector = normalize(LP.xyz + fL);
	v.x = dot (halfVector, T);
	v.y = dot (halfVector, B);
	v.z = dot (halfVector, N);
	fH = halfVector;
    
    vec3 v3LightDir = fL;
	vec3 v3Pos = vPosition.xyz;
	vec3 v3Ray = v3Pos - v3CameraPos;
	float fFar = length(v3Ray);
	v3Ray /= fFar;
	
	float fNear = fInnerRadius - fCameraHeight;
	
	vec3 v3Start = v3CameraPos + v3Ray * fNear;
	fFar -= fNear;
	float fStartAngle = dot(v3Ray, v3Start) / fOuterRadius;
	float fStartDepth = exp(fScaleOverScaleDepth * (fInnerRadius - fCameraHeight));
	float fStartOffset = fStartDepth * scale(fStartAngle);
	
	float fSampleLength = fFar / fSamples;
	float fScaledLength = fSampleLength * fScale;
	vec3 v3SampleRay = v3Ray * fSampleLength;
	vec3 v3SamplePoint = v3Start + v3SampleRay * 0.5;
	
	vec3 v3FrontColor = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < nSamples; i++) {
		float fHeight = length(v3SamplePoint);
		float fDepth = exp(fScaleOverScaleDepth * (fInnerRadius - fHeight));
		float fLightAngle = dot(v3LightDir, v3SamplePoint) / fHeight;
		float fCameraAngle = dot(v3Ray, v3SamplePoint) / fHeight;
		float fScatter = (fStartOffset + fDepth * (scale(fLightAngle)) - scale(fCameraAngle));
		vec3 v3Attenuate = exp(-fScatter * v3InvWavelength * fKr4PI + fKm4PI);
		v3FrontColor += v3Attenuate * (fDepth * fScaledLength);
		v3SamplePoint += v3SampleRay;
	}
	
	frontColor = v3FrontColor * (v3InvWavelength * fKrESun);
	frontSecondaryColor = v3FrontColor * fKmESun;
	v3Direction = v3CameraPos - v3Pos;
    
    TexCoords = vPosition;
}