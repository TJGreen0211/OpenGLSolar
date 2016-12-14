#version 410

uniform vec4 LightPosition;
uniform vec3 camPosition;
uniform float g;
uniform float g2;
uniform float time;

in vec3 direction;
in vec4 rayleighColor;
in vec4 mieColor;
in vec3 testFrag;

out vec4 FragColor;

// Mie
// g : ( -0.75, -0.999 )
//      3 * ( 1 - g^2 )               1 + c^2
// F = ----------------- * -------------------------------
//      2 * ( 2 + g^2 )     ( 1 + g^2 - 2 * g * c )^(3/2)
float miePhase( float g, float c, float cc ) {
	float gg = g * g;
	
	float a = ( 1.0 - gg ) * ( 1.0 + cc );

	float b = 1.0 + gg - 2.0 * g * c;
	b *= sqrt( b );
	b *= 2.0 + gg;	
	
	return 1.5 * a / b;
}

// Reyleigh
// g : 0
// F = 3/4 * ( 1 + c^2 )
float rayleighPhase( float cc ) {
	return 0.75 * ( 1.0 + cc );
}

void main (void)
{
	vec4 frag;
	float t = direction.x + direction.y + direction.z;
	vec3 lightPos = LightPosition.xyz;
	float fCos = dot(lightPos, direction)/ length(direction);
	float fCos2 = fCos*fCos;
	FragColor = rayleighPhase(fCos2) * rayleighColor + miePhase(fCos, fCos2, g) * mieColor;
	FragColor.a = FragColor.b;
	
	FragColor = vec4(testFrag, 1.0);
}