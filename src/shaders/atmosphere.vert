#version 410

in vec4 vPosition;
in vec3 vNormal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 camPosition;
uniform float fInnerRadius;
uniform float fOuterRadius;

const float PI = 3.14159265359;
vec3 invWaveLength = vec3(0.3, 0.7, 1.0);
vec3 lightPos = normalize(vec3(0.0, 0.0, 1.0));
float K_R = 0.166;
float K_M = 0.0025;
float E = 14.3;
float scaleOver = 4.0 / (fOuterRadius - fInnerRadius);
float fScale = 1.0 / (fOuterRadius - fInnerRadius);
int numInScatter = 10;
float fNumInScatter = 10.0;

out vec3 color;
out vec3 secondaryColor;
out vec3 direction;
out vec3 testFrag;

float scale(float fCos)	
{	
	float x = 1.0 - fCos;
	return 0.25 * exp(-0.00287 + x*(0.459 + x*(3.83 + x*(-6.80 + x*5.25))));	
}

vec3 rayDir(vec3 cam) {
	vec3 ray = (model*vPosition).xyz - mat3(inverse(view))*cam;
	return normalize(ray);
}

float getIntersection(vec3 cam, vec3 dir, float r) {
	float camH2 = length(cam)*length(cam) ;
	float B = 2.0 * dot(cam, dir);	
	float C = camH2 - r*r;	
	float fDet = max(0.0, B*B - 4.0 * C);	
	return 0.5 * (-B - sqrt(fDet));	
}

void main()
{	
	vec3 ray = rayDir(camPosition);
	float near = getIntersection(camPosition, ray, fOuterRadius);
	vec3 start = camPosition + ray * near;
	float far = length(ray);
	far -= near;
	float startAngle = dot(ray, start) / fOuterRadius;
	float startDepth = exp(-1.0 / 0.25);
	float startOffset = startDepth*scale(startAngle);
	
	float sampleLength = far * fNumInScatter;
	float scaledLength = sampleLength * fScale;
	vec3 sampleRay = ray * sampleLength;
	vec3 samplePoint = start + sampleRay * 0.5;
	
	vec3 frontColor = vec3(0.0);
	for(int i = 0; i < numInScatter; i++) 
	{
		float height = length(samplePoint);
		float depth = exp((0.25/fScale) * (fInnerRadius - height));
		float lightAngle = dot(lightPos, samplePoint) / height;
		float cameraAngle = dot(ray, samplePoint) / height;
		float scatter = (startOffset + depth*(scale(lightAngle) - scale(cameraAngle)));
		vec3 attenuate = exp(-scatter * (invWaveLength * (K_R*PI * 4.0 + K_M*PI * 4.0)));
		frontColor += attenuate * (depth * scaledLength);
		samplePoint += sampleRay;
		
		if(isnan(frontColor.x) || isinf(frontColor.x) || isnan(frontColor.y) || isinf(frontColor.y))
			testFrag = vec3(1.0, 0.0, 0.0);
		else if(frontColor.x == 0.0 || frontColor.y == 0.0 || frontColor.z == 0.0)
			testFrag = vec3(0.0, 0.0, 1.0);
		else
			testFrag = vec3(0.0, 1.0, 0.0);
	}
	
	secondaryColor = frontColor * K_M*E;
	color = frontColor * (invWaveLength * K_R*E);
	direction = camPosition - vPosition.xyz;
	
	gl_Position = projection*view*model*vPosition;
}