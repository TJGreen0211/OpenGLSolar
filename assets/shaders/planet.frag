#version 410

// Per-fragment lighting: the world-space tangent basis and world position are
// interpolated from planet.vert, and L/V/H are built here, so the specular
// response no longer depends on mesh or quad-tree patch density.

in vec3 fT;
in vec3 fB;
in vec3 fN;
in vec3 vWorldPos;
in vec4 TexCoords;
in vec3 vLocalPos;
out vec4 FragColor;

uniform vec4 ambientProduct, diffuseProduct, specularProduct;
uniform vec4 lightPos;   // world space; w != 0 -> positional, w == 0 -> dir
uniform vec3 camPos;     // world space
uniform float shininess; // base (land) lobe exponent
// 1 on bodies with real oceans (Earth): blue-dominant albedo pixels get a
// water F0 and a much tighter specular lobe. 0 everywhere else.
uniform float waterSpec;

uniform sampler2D tex;
uniform sampler2D normalTex;

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

void main()
{
	vec3 pos = normalize(TexCoords.xyz);
	vec2 longlat = vec2((atan(pos.y, pos.x) / 3.1415926 + 1.0) * 0.5,
	                           (asin(clamp(pos.z, -1.0, 1.0)) / 3.1415926 + 0.5));
	vec3 color = vec3(texture(tex, longlat));
	vec3 detail = 2.0 * vec3(texture(normalTex, longlat)) - 1.0;

	// Normal fade: once the surface minifies past a texel per screen pixel,
	// scale the tangent-space slopes toward flat so minified normal detail
	// stops shimmering. mipLevel is the texel footprint (in texels) covered by
	// one screen pixel.
	vec2 texDim = vec2(textureSize(normalTex, 0));
	vec2 mipFootprint = abs(vec2(fwidth(longlat.x), fwidth(longlat.y)) * texDim);
	float spread = max(max(mipFootprint.x, mipFootprint.y), 1.0);
	detail.xy /= spread;
	detail = normalize(detail);

	// Tangent -> world. Re-orthogonalise T against the interpolated N so the
	// basis stays orthonormal despite interpolation.
	vec3 T = normalize(fT);
	vec3 baseN = normalize(fN);
	T = normalize(T - dot(T, baseN) * baseN);
	vec3 B = cross(baseN, T);
	vec3 N = normalize(detail.x * T + detail.y * B + detail.z * baseN);

	// Half-Anglet convention: V points fragment -> camera (toward the viewer)
	// so that H = normalize(L + V) is the true half vector of the specular
	// lobe. (The old vertex shader used camera -> fragment and subtracted.)
	vec3 V = normalize(camPos - vWorldPos);
	vec3 L = (lightPos.w != 0.0) ? normalize(lightPos.xyz - vWorldPos)
	                             : normalize(lightPos.xyz);

	float NdotL = max(dot(N, L), 0.0);
	vec3 H = normalize(L + V);
	float NdotH = max(dot(N, H), 0.0);
	float VdotH = max(dot(V, H), 0.0);

	// Specular: land is a dim dielectric (specularProduct ~ F0 of rock); on
	// ocean-bearing bodies, blue-dominant pixels become water (F0 ~ 0.05,
	// tight lobe). Fresnel keeps the highlight physically scaled: near zero
	// head-on, rising toward grazing, and zero on the night side via NdotL.
	float ocean = smoothstep(0.02, 0.15, color.b - max(color.r, color.g)) * waterSpec;
	vec3 f0 = mix(specularProduct.rgb, vec3(0.05), ocean);
	float shin = mix(shininess, 400.0, ocean);
	vec3 fresnel = f0 + (vec3(1.0) - f0) * pow(1.0 - VdotH, 5.0);
	float specularNorm = (shin + 8.0) / (8.0 * PI);

	vec3 ambient = ambientProduct.rgb * color;
	vec3 diffuse = NdotL * diffuseProduct.rgb * color;
	vec3 specular = vec3(0.0);
	if (dot(N, L) > 0.0) {
		specular = fresnel * specularNorm * pow(NdotH, shin);
	}

	FragColor = vec4(ambient + diffuse + specular, 1.0);

	// out-scattering: the dome pass adds the in-scattered sky on top of this
	// (its ray is clipped at the surface), so the surface itself is only
	// attenuated here -- no double counting, inside or outside the shell
	if (hasAtmosphere > 0.5) {
		FragColor.rgb *= atmosphericExtinction();
	}
}
