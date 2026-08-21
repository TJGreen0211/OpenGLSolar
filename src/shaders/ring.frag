#version 410

in vec3 fWorldPos;
in vec3 fNormal;
in vec2 fTexCoord;
in vec3 fCamPos;

out vec4 FragColor;

uniform sampler2D tex;
uniform vec3 planetCenter;
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

	// The sun sits at the origin of the system: rings brighten when the
	// sun is high above the ring plane (planet seasons)
	vec3 L = normalize(-fWorldPos);
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
}
