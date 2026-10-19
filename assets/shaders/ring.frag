#version 410

in vec3 fWorldPos;
in vec3 fNormal;
in vec2 fTexCoord;
in vec3 fCamPos;
in float fViewDist;

out vec4 FragColor;

uniform vec2 uLogDepth; // log2(near), log2(far)-log2(near): see Common.c
uniform sampler2D tex;
uniform vec3 planetCenter;   // camera-relative planet centre
uniform vec3 uSunPos;        // camera-relative sun (universe origin)
uniform float planetRadius;

void main()
{
	vec4 ringColor = texture(tex, fTexCoord);
	if(ringColor.a < 0.01)
		discard;

	// Face the (flat) normal towards the camera before lighting
	vec3 N = fNormal;
	vec3 V = normalize(fCamPos - fWorldPos);
	if(dot(N, V) < 0.0)
		N = -N;

	// Sun (camera-relative): rings brighten when the sun is high above the
	// ring plane (planet seasons)
	vec3 L = normalize(uSunPos - fWorldPos);
	float light = 0.55 + 0.45*max(dot(N, L), 0.0);

	// Planet shadow: does the ray from this fragment to the sun pass
	// through the planet sphere?
	float shadow = 1.0;
	vec3 oc = planetCenter - fWorldPos;
	float t = dot(oc, L);
	if(t > 0.0)
	{
		vec3 d = fWorldPos + L*t - planetCenter;
		float g = dot(d, d)/(planetRadius*planetRadius);
		if(g < 1.0)
			shadow = 0.15 + 0.85*g;
	}

	FragColor = vec4(ringColor.rgb*light*shadow, ringColor.a);
	// Depth-tested but not depth-writing (CPU side): the test value must
	// still be in the log-depth scale everything else uses.
	gl_FragDepth = clamp((log2(max(fViewDist, 0.0001)) - uLogDepth.x) / uLogDepth.y,
	                     0.0, 1.0);
}
