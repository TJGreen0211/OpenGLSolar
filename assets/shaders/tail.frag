#version 410

in vec2  fUv;
in float fAlpha;
in float fViewDist;

uniform float uTailAlpha;  // global brightness (proportional to 1/r^2)
uniform vec3  uColor;
uniform vec2  uLogDepth;   // log2(near), log2(far)-log2(near): see Common.c

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
	// Depth-tested but not depth-writing (CPU side): the test value must
	// still be in the log-depth scale everything else uses.
	gl_FragDepth = clamp((log2(max(fViewDist, 0.0001)) - uLogDepth.x) / uLogDepth.y,
	                     0.0, 1.0);
}
