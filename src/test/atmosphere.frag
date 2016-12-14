#version 410

in vec3 fN;
in vec3 fL;
in vec3 fE;
in vec3 fH;
in vec4 TexCoords;
out vec4 FragColor;

uniform vec4 LightPosition;
uniform float g;
uniform float g2;
uniform vec3 camPosition;
uniform float time;

// math const
const float PI = 3.14159265359;
const float DEG_TO_RAD = PI / 180.0;
const float MAX = 10000.0;

// scatter const
const float K_R = 0.166;
const float K_M = 0.0025;
const float E = 14.3; 						// light intensity
const vec3  C_R = vec3( 0.3, 0.7, 1.0 ); 	// 1 / wavelength ^ 4
const float G_M = -0.85;					// Mie g

uniform float fInnerRadius;
uniform float fOuterRadius;
float SCALE_H = 4.0 / ( fOuterRadius - fInnerRadius );
float SCALE_L = 1.0 / ( fOuterRadius - fInnerRadius );

int NUM_OUT_SCATTER = 10;
float FNUM_OUT_SCATTER = 10.0;

int NUM_inScatter = 10;
float FNUM_inScatter = 10.0;

// ray intersects sphere
// e = -b +/- sqrt( b^2 - c )
vec2 rayVSSphere( vec3 p, vec3 dir, float r ) {
	float b = dot( p, dir );
	float c = dot( p, p ) - r * r;
	
	float d = b * b - c;
	if ( d < 0.0 ) {
		return vec2( MAX, -MAX );
	}
	d = sqrt( d );
	
	return vec2( -b - d, -b + d );
}

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
float reyleighPhase( float cc ) {
	return 0.75 * ( 1.0 + cc );
}

float density( vec3 p ){
	return exp( -( length( p ) - fInnerRadius ) * SCALE_H );
}

float optic( vec3 p, vec3 q ) {
	vec3 step = ( q - p ) / FNUM_OUT_SCATTER;
	vec3 v = p + step * 0.5;
	
	float sum = 0.0;
	for ( int i = 0; i < NUM_OUT_SCATTER; i++ ) {
		sum += density( v );
		v += step;
	}
	sum *= length( step ) * SCALE_L;
	
	return sum;
}

vec3 inScatter( vec3 o, vec3 dir, vec2 e, vec3 l ) {
	float len = ( e.y - e.x ) / FNUM_inScatter;
	vec3 step = dir * len;
	vec3 p = o + dir * e.x;
	vec3 v = p + dir * ( len * 0.5 );

	vec3 sum = vec3( 0.0 );
	for ( int i = 0; i < NUM_inScatter; i++ ) {
		vec2 f = rayVSSphere( v, l, fOuterRadius );
		vec3 u = v + l * f.y;
		
		float n = ( optic( p, v ) + optic( v, u ) ) * ( PI * 4.0 );
		
		sum += density( v ) * exp( -n * ( K_R * C_R + K_M ) );

		v += step;
	}
	sum *= len * SCALE_L;
	
	float c  = dot( dir, -l );
	float cc = c * c;
	
	return sum * ( K_R * C_R * reyleighPhase( cc ) + K_M * miePhase( G_M, c, cc ) ) * E;
}

mat3 rot3xy( vec2 angle ) {
	vec2 c = cos( angle );
	vec2 s = sin( angle );
	
	return mat3(
		c.y      ,  0.0, -s.y,
		s.y * s.x,  c.x,  c.y * s.x,
		s.y * c.x, -s.x,  c.y * c.x
	);
}

float rayDir(vec3 v3Pos, vec3 v3Ray, float fDistance2, float fRadius2)
{
   float B = 2.0 * dot(v3Pos, v3Ray);
   float C = fDistance2 - fRadius2;
   float fDet = max(0.0, B*B - 4.0 * C);
   return 0.5 * (-B - sqrt(fDet));
}


void main()
{	
	vec3 dir = fL - TexCoords.xyz;//normalize(light);
    vec3 eye = vec3( 0.0, 0.0, 2.4 );
    vec3 l = vec3( 0, 0, 1 );
    
    mat3 rot = rot3xy( vec2( 0.0, time * 0.5 ) );
	//dir = rot * dir;
	//eye = rot * eye;
	
    vec2 e = rayVSSphere( eye, dir, fOuterRadius );
	if ( e.x > e.y ) {
		discard;
	}
	
	vec2 f = rayVSSphere( eye, dir, fInnerRadius );
	e.y = min( e.y, f.x );

	vec3 I = inScatter( eye, dir, e, l );
	
	FragColor = vec4( I, 1.0 );
	FragColor.a = FragColor.b;
}
