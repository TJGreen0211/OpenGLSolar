#version 410

in vec2  fUv;
in float fAlpha;

uniform float uTailAlpha;  // global brightness (proportional to 1/r^2)
uniform vec3  uColor;

out vec4 FragColor;

void main()
{
	// fUv is in -0.5..0.5, so length*2 gives 0 at the centre and 1 at the
	// square edge (1.414 at a corner). Smooth radial falloff -> soft disc.
	float d = length(fUv) * 2.0;
	float fall = smoothstep(1.0, 0.0, d);

	float a = fAlpha * uTailAlpha * fall;
	if(a < 0.003)
		discard;

	// Drawn with glBlendFunc(GL_SRC_ALPHA, GL_ONE): dst += uColor * a.
	FragColor = vec4(uColor, a);
}
