#version 410

in float fViewDist;

out vec4 FragColor;

uniform float uAlpha;
uniform vec2 uLogDepth; // log2(near), log2(far)-log2(near): see Common.c

void main()
{
	FragColor = vec4(1.0, 1.0, 1.0, uAlpha);
	// Depth-tested but not depth-writing (CPU side): the test value must
	// still be in the log-depth scale everything else uses.
	gl_FragDepth = clamp((log2(max(fViewDist, 0.0001)) - uLogDepth.x) / uLogDepth.y,
	                     0.0, 1.0);
}
