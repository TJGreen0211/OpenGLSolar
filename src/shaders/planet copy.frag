#version 410

in vec3 fN;
in vec3 fL;
in vec3 fE;
in vec3 fH;
in vec4 TexCoords;
out vec4 FragColor;

uniform vec4 AmbientProduct, DiffuseProduct, SpecularProduct;
uniform float Shininess;

uniform sampler2D tex;
uniform sampler2D normalTex;

//Constants
const float PI = 3.14159265359;
const float DEG_TO_RAD = PI / 180.0;
const float MAX = 10000.0;

//Scattering constants
const float E = 14.3; 
const vec3  C_R = vec3( 0.3, 0.7, 1.0 ); 	// 1 / wavelength ^ 4
const float G_M = -0.85;					// Mie g
uniform float fKrESun;
uniform float fKmESun;
uniform float fInnerRadius;
uniform float fOuterRadius;
uniform float g;
uniform float g2;
uniform vec3 v3CameraPos;

float scaleH = 4.0 / (fOuterRadius - fInnerRadius);
float scaleL = 1.0 / (fOuterRadius - fInnerRadius);

int numInScatter = 10;
float fNumInScatter = 10.0;

int numOutScatter = 10;
float fNumOutScatter = 10.0;

/*
//Calculate ray direction
vec3 rayDirection(float fov, vec2 size, vec2 pos) {
	vec2 xy = pos - size * 0.5;
	float cotHalfFov
}
*/

//Ray intersects sphere
vec2 rayVSSphere(vec3 p, vec3 dir, float r) {
	float b = dot(p, dir);
	float c = dot(p, p) - r * r;
	float d = b * b - c;
	if(d < 0.0) {
		return vec2(MAX, -MAX);
	}
	d = sqrt(d);
	return vec2(-b - d, -b + d);
}

//Mie
float getMiePhase(float fCos, float fCos2, float g, float g2)
{
	return 1.5 * ((1.0 - g2) / (2.0 + g2)) * (1.0 + fCos2) / pow(1.0 + g2 - 2.0 * g * fCos, 1.5);
}

//Reyleigh
float getReyleighPhase(float fCos2)
{
	return 0.75 + 0.75 * fCos2;
}

float density(vec3 p) {
	return exp(-(length(p) - fInnerRadius) * scaleH);
}

float optic(vec3 p, vec3 q) {
	vec3 step = (q - p) / fNumOutScatter;
	vec3 v = p + step * 0.5;
	
	float sum = 0.0;
	for(int i = 0; i < numOutScatter; i++) {
		sum += density(v);
		v += step;
	}
	sum *= length(step) * scaleL;
	return sum;
}

vec3 inScatter(vec3 o, vec3 dir, vec2 e, vec3 l) {
	float len = (e.y - e.x) / fNumInScatter;
	vec3 step = dir * len;
	vec3 p = o + dir * e.x;
	vec3 v = p + dir * (len * 0.5);
	vec3 sum = vec3(0.0);
	
	for(int i = 0; i < numInScatter; i++)
	{
		vec2 f = rayVSSphere(v, l, fOuterRadius);
		vec3 u = v + l * f.y;
		float n = ( optic( p, v ) + optic( v, u ) ) * ( PI * 4.0 );
		sum += density(v) * exp(-n * (fKrESun * C_R + fKmESun));
		v += step;
	}
	sum *= len * scaleL;
	
	float c = dot(dir, -l);
	float cc = c * c;
	return sum * (fKrESun * C_R * getReyleighPhase( cc ) + fKmESun * getMiePhase(g, g2, c, cc)) * E;
}

/*
vec3 rayDirection(float fov, vec2 pos) {
	vec2 xy = pos;
	
}*/

void main()
{	
	vec2 longlat = vec2((atan(TexCoords.y, TexCoords.x) / 3.1415926 + 1.0) * 0.5,
                                  (asin(TexCoords.z) / 3.1415926 + 0.5));
    vec3 color = vec3(texture(tex, longlat));
    vec3 N = 2.0*vec3(texture(normalTex, longlat))-1.0;
    N = normalize(fN);
    
	//vec3 N = normalize(fN);
	//vec3 H = normalize( L + E );
    //vec3 E = fE;
    
    float Kd = max(dot(fL, N), 0.0);
    float Ks = pow(max(dot(N, fH), 0.0), Shininess);
    
    vec3 ambient = AmbientProduct.xyz * color;
    vec3 diffuse = Kd * DiffuseProduct.xyz * color;
    vec3 specular = Ks * SpecularProduct.xyz;

    // discard the specular highlight if the light's behind the vertex
    if( dot(fL, N) < 0.0 ) {
		specular = vec3(0.0, 0.0, 0.0);
    }
    
    vec3 eye = vec3( 0.0, 0.0, 2.4 );
    vec3 dir = fE;
    
    vec2 e = rayVSSphere(fE, fE, fOuterRadius);
    vec2 f = rayVSSphere(fE, fE, fInnerRadius);
    e.y = min( e.y, f.x );
    vec3 I = inScatter( fE, fE, e, fL);

    FragColor = vec4(ambient + diffuse + specular, 1.0);//vec4( I, 1.0 );//
    //FragColor.a = 1.0;
}
