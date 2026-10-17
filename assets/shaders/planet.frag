#version 410

in vec3 fN;
in vec3 fL;
in vec3 fE;
in vec3 fH;
in vec4 TexCoords;
in vec3 vLocalPos;
out vec4 FragColor;

uniform vec4 ambientProduct, diffuseProduct, specularProduct;
uniform float shininess;

uniform sampler2D tex;
uniform sampler2D normalTex;
uniform sampler2D depthMap;

// Atmospheric extinction (out-scattering): the body's own atmosphere dims the
// surface by the transmittance along the surface->camera path through the
// shell. Constants and the 4*PI optical-depth convention must match
// atmosphere.frag so dome in-scatter and planet fog agree.
uniform float hasAtmosphere;
uniform float fInnerRadius;
uniform float fOuterRadius;
uniform vec3 C_R;
uniform vec3 camLocalPos;

const float PI = 3.14159265359;
const float MAX = 10000.0;
const float K_R = 0.166;
const float K_M = 0.0025;
const int NUM_EXTINCT_STEPS = 5;

float height_scale = 0.02;

vec2 shellIntersection(vec3 p, vec3 dir, float radius)
{
	float b = dot(p, dir);
	float c = dot(p, p) - radius * radius;
	float d = b * b - c;
	if (d < 0.0) {
		return vec2(MAX, -MAX);
	}
	d = sqrt(d);
	return vec2(-b - d, -b + d);
}

// Optical depth along eye->surface segment [e0, e1] of the view ray, sampled
// like optic() in atmosphere.frag (midpoint rule, 5 steps).
float viewOpticalDepth(vec3 eye, vec3 dir, float e0, float e1)
{
	float scaleH = 4.0 / (fOuterRadius - fInnerRadius);
	float scaleL = 1.0 / (fOuterRadius - fInnerRadius);
	float len = (e1 - e0) / float(NUM_EXTINCT_STEPS);
	vec3 step = dir * len;
	vec3 v = eye + dir * (e0 + len * 0.5);

	float sum = 0.0;
	for (int i = 0; i < NUM_EXTINCT_STEPS; i++) {
		sum += exp(-(length(v) - fInnerRadius) * scaleH);
		v += step;
	}
	return sum * length(step) * scaleL * (4.0 * PI);
}

vec3 atmosphericExtinction()
{
	vec3 dir = normalize(vLocalPos - camLocalPos);
	vec2 e = shellIntersection(camLocalPos, dir, fOuterRadius);
	if (e.x > e.y) {
		return vec3(1.0); // view ray misses the shell entirely
	}
	vec2 f = shellIntersection(camLocalPos, dir, fInnerRadius);
	e.y = min(e.y, f.x);
	// camera inside the shell: the path starts at the camera
	e.x = max(e.x, 0.0);
	if (e.y <= e.x) {
		return vec3(1.0);
	}
	float tau = viewOpticalDepth(camLocalPos, dir, e.x, e.y);
	return exp(-(K_R * C_R + K_M) * tau);
}

vec2 parallaxMapping(vec2 texCoords)
{
	float height = texture(depthMap, texCoords).r;
	vec2 p = fE.xy / fE.z * (height * height_scale);
	// fE is the camera -> fragment direction, so the visible point is
	// displaced towards the camera: offset is added, not subtracted
	return texCoords + p;
}

void main()
{
	vec3 pos = normalize(TexCoords.xyz);
	vec2 longlat = vec2((atan(pos.y, pos.x) / 3.1415926 + 1.0) * 0.5,
                                  (asin(clamp(pos.z, -1.0, 1.0)) / 3.1415926 + 0.5));
    vec2 D = parallaxMapping(longlat);
    // sample color and normal at the same (parallaxed) coordinates
    vec3 color = vec3(texture(tex, D));
    vec3 N = 2.0*vec3(texture(normalTex, D))-1.0;
    
    N = normalize(N);
    
    float Kd = max(dot(fL, N), 0.0);
    float Ks = pow(max(dot(N, fH), 0.0), shininess);
    
    vec3 ambient = ambientProduct.xyz * color;
    vec3 diffuse = Kd * diffuseProduct.xyz * color;
    vec3 specular = Ks * specularProduct.xyz;

    // discard the specular highlight if the light's behind the vertex
    if( dot(fL, N) < 0.0 ) {
		specular = vec3(0.0, 0.0, 0.0);
    }
    
    FragColor = vec4(ambient + diffuse + specular, 1.0);

    // out-scattering: the dome pass adds the in-scattered sky on top of this
    // (its ray is clipped at the surface), so the surface itself is only
    // attenuated here -- no double counting, inside or outside the shell
    if (hasAtmosphere > 0.5) {
        FragColor.rgb *= atmosphericExtinction();
    }
}
