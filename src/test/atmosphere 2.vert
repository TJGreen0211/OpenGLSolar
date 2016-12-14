#version 410

in vec4 vPosition;
in vec3 vNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec4 LightPosition;

uniform vec3 camPosition;
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
uniform float fScaleDepth;
uniform float fScaleOverScaleDepth;

const int nSamples = 5;
const float fSamples = 5.0;

out vec3 direction;
out vec4 rayleighColor;
out vec4 mieColor;

float fInvScaleDepth = (1.0 / fScaleDepth);
out vec3 testFrag;

float getNearIntersection(vec3 v3Pos, vec3 v3Ray, float fDistance2, float fRadius2)
{
   float B = 2.0 * dot(v3Pos, v3Ray);
   float C = fDistance2 - fRadius2;
   float fDet = max(0.0, B*B - 4.0 * C);
   return 0.5 * (-B - sqrt(fDet));
}

float scale(float fCos)
{
	float x = 1.0 - fCos;
	return fScaleDepth * exp(-0.00287 + x*(0.459 + x*(3.83 + x*(-6.80 + x*5.25))));
}

void main()
{	
	mat4 ModelView = view*model;
	vec3 camPosition = camPosition*inverse(mat3(model));
	vec3 lightPos = (LightPosition-model*vPosition).xyz;
	vec3 pos = (ModelView*vPosition).xyz;
	vec3 ray = pos - lightPos;
	float far = length(ray);
	ray /= far;
	
	float near = getNearIntersection(lightPos, ray, fCameraHeight2, fOuterRadius2);
	
	vec3 start = lightPos + ray * near;
	far -= near;
	
	float startAngle = dot(ray, start) / fOuterRadius;
	float startDepth = exp(fScaleOverScaleDepth * (fInnerRadius - fCameraHeight));
	float startOffset = startDepth * scale(startAngle);
	
	float sampleLength = far / fSamples;
	float scaledLength = sampleLength * fScale;
	vec3 sampleRay = ray * sampleLength;
	vec3 samplePoint = start + sampleRay * 0.5;
	
	vec3 frontColor = vec3(0.0, 0.0, 0.0);
	for(int i = 0; i < nSamples; i++)
	{
		float height = length(samplePoint);
		float depth = exp(fScaleOverScaleDepth * (fInnerRadius - height));
		float lightAngle = dot(lightPos, samplePoint) / height;
		float cameraAngle = dot(ray, samplePoint) / height;
		float scatter = (startOffset + depth * (scale(lightAngle) - scale(cameraAngle)));
		vec3 attenuate = exp(-scatter * (v3InvWavelength * fKr4PI + fKm4PI));
		frontColor += attenuate * (depth * scaledLength);

		if(isnan(scale(frontColor.x)) || isinf(scale(frontColor.x)) || isnan(scale(frontColor.y)) || isinf(scale(frontColor.y)))
			testFrag = vec3(1.0, 0.0, 0.0);
		else if(frontColor.x == 0.0 && frontColor.y == 0.0 && frontColor.z == 0.0)
			testFrag = vec3(0.0, 0.0, 1.0);
		else
			testFrag = vec3(0.0, 1.0, 0.0);
	}
	
	
	mieColor = vec4(frontColor * fKmESun, 1.0);
	rayleighColor = vec4(frontColor * (v3InvWavelength * fKrESun), 1.0);
	direction = lightPos - pos;
	
	gl_Position = projection*ModelView*vPosition;
}