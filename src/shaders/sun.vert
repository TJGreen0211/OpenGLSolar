#version 410

in vec3 vPosition;

out vec2 starOffset;

uniform mat4 view;
uniform mat4 projection;
uniform float sunRadius;

void main()
{
	// Row-vector convention (matches planet.vert / the rest of the app):
	// the sun sits at the world origin.
	vec4 sunView = vec4(0.0, 0.0, 0.0, 1.0) * view;
	if( sunView.z >= 0.0 )
	{
		// At or behind the camera image plane the sun would be degenerate,
		// so emit everything off-screen (a zero-area point that clips away).
		gl_Position = vec4(2.0, 0.0, 0.0, 1.0);
		starOffset = vec2(0.0);
		return;
	}

	vec4 clip = sunView * projection;

	// Size the billboard by the sun's *projected* radius so it scales with
	// distance like any other object. Project the origin and a point
	// sunRadius away in view-space +X (same depth); the difference in their
	// NDC x is the on-screen half-size of a sphere of radius sunRadius at the
	// sun's depth.
	vec4 clipR = (sunView + vec4(sunRadius, 0.0, 0.0, 0.0)) * projection;
	float ndcRadius = abs( clipR.x / clipR.w - clip.x / clip.w );

	// Spread the quad corners (vPosition.x/y in [-1,1]) by that NDC half-size.
	// Multiply by clip.w so the w-divide in projection is exact.
	clip.x += vPosition.x * ndcRadius * clip.w;
	clip.y += vPosition.y * ndcRadius * clip.w;

	gl_Position = clip;
	starOffset = vPosition.xy * 2.0;
}
