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
in float fViewDist;   // view-space distance ahead of the camera
out vec4 FragColor;

uniform vec2 uLogDepth; // log2(near), log2(far)-log2(near): see Common.c

uniform vec4 ambientProduct, diffuseProduct, specularProduct;
uniform vec4 lightPos;   // world space; w != 0 -> positional, w == 0 -> dir
uniform vec3 camPos;     // world space
uniform float shininess; // base (land) lobe exponent
// 1 on bodies with real oceans (Earth): blue-dominant albedo pixels get a
// water F0 and a much tighter specular lobe. 0 everywhere else.
uniform float waterSpec;

uniform sampler2D tex;
uniform sampler2D normalTex;

// Atmospheric extinction dims the surface by transmittance along the
// surface-to-camera path. The atmosphere dome pass adds in-scattered light
// over both the limb and the planet disc; adding it here as well would count
// that radiance twice.
uniform float hasAtmosphere;
uniform float fInnerRadius;
uniform float fOuterRadius;
uniform vec3 C_R;
uniform vec3 camLocalPos;
// Where the vLocalPos frame origin sits relative to the shell centre: zero
// for the planet itself; for a moon drawn through this shader it is the
// moon's offset from its parent centre, applying the parent's shell.
uniform vec3 shellOffset;

const float PI = 3.14159265359;
// Keep the miss sentinel beyond the largest supported shell radius.
const float MAX = 1e30;
const float K_R = 0.166;
const float K_M = 0.0025;
const int NUM_ATMO_STEPS = 5;

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

float optic(vec3 p, vec3 q, float scaleH, float scaleL)
{
	vec3 step = (q - p) / float(NUM_ATMO_STEPS);
	vec3 v = p + step * 0.5;
	float sum = 0.0;
	for (int i = 0; i < NUM_ATMO_STEPS; i++) {
		sum += exp(-(length(v) - fInnerRadius) * scaleH);
		v += step;
	}
	return sum * length(step) * scaleL;
}

vec3 applyAtmosphere(vec3 color)
{
	float scaleH = 4.0 / (fOuterRadius - fInnerRadius);
	float scaleL = 1.0 / (fOuterRadius - fInnerRadius);

	// Ray in the shell-centred frame: eye = camera, fragment at distance tFrag.
	vec3 eye = camLocalPos;
	vec3 fragS = vLocalPos + shellOffset;
	vec3 ray = fragS - eye;
	float tFrag = length(ray);
	vec3 dir = ray / max(tFrag, 1e-6);

	vec2 e = shellIntersection(eye, dir, fOuterRadius);
	if (e.x > e.y) {
		return color; // view ray misses the shell entirely
	}
	vec2 f = shellIntersection(eye, dir, fInnerRadius);
	// Clip the path at the shell exit, the inner sphere, and the actual
	// fragment hit: the tFrag term handles a moon in front of its parent's
	// shell (no path -> no fog) and displaced surfaces, where f.x would
	// overshoot or fall short of the visible surface.
	e.y = min(e.y, min(f.x, tFrag));
	// camera inside the shell: the path starts at the camera
	e.x = max(e.x, 0.0);
	if (e.y <= e.x) {
		return color;
	}

	float tau = optic(eye + dir * e.x, eye + dir * e.y, scaleH, scaleL) * (4.0 * PI);
	color *= exp(-(K_R * C_R + K_M) * tau);

	return color;
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

	// Surface extinction only; the atmosphere dome owns all in-scattered
	// radiance so the scattering is not composited twice over the body.
	if (hasAtmosphere > 0.5) {
		FragColor.rgb = applyAtmosphere(FragColor.rgb);
	}

	// Logarithmic depth (dynamic near/far + log mapping): the scene spans
	// camera-surface to system-wide distances, and a perspective [near,far]
	// depth cannot resolve both.
	gl_FragDepth = clamp((log2(max(fViewDist, 0.0001)) - uLogDepth.x) / uLogDepth.y,
	                     0.0, 1.0);
}
