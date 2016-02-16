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
uniform sampler2D hdrBuffer;
uniform float exposure;
uniform float systemTime;

float snoise(vec3 uv, float res)	// by trisomie21
{
	const vec3 s = vec3(1e0, 1e2, 1e4);
	
	uv *= res;
	
	vec3 uv0 = floor(mod(uv, res))*s;
	vec3 uv1 = floor(mod(uv+vec3(1.), res))*s;
	
	vec3 f = fract(uv); f = f*f*(3.0-2.0*f);
	
	vec4 v = vec4(uv0.x+uv0.y+uv0.z, uv1.x+uv0.y+uv0.z,
		      	  uv0.x+uv1.y+uv0.z, uv1.x+uv1.y+uv0.z);
	
	vec4 r = fract(sin(v*1e-3)*1e5);
	float r0 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
	
	r = fract(sin((v + uv1.z - uv0.z)*1e-3)*1e5);
	float r1 = mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y);
	
	return mix(r0, r1, f.z)*2.-1.;
}

float brightness = 0.1;

void mainImage()
{
    if( brightness < 0.15 ) {
        brightness	= max( ( cos(10.0) * 0.25 + sin(1.0) * 0.25 ), 0.1); 
    }
	float radius		= 0.24 + brightness * 0.2;
	float invRadius 	= 1.0/radius;
	
	vec3 orange			= vec3( 0.2, 0.65, 0.5 );
	vec3 orangeRed		= vec3( 0.1, 0.25, 0.81 );
	float time		= systemTime * 0.1;
	float aspect	= 1200/800;
	vec2 uv			= TexCoords.xy / vec2(1200, 800);
	vec2 p 			= -0.5 + uv;
	p.x *= aspect;

	float fade		= pow( length( 2.0 * p ), 0.5 );
	float fVal1		= 1.0 - fade;
	float fVal2		= 1.0 - fade;
	
	float angle		= atan( p.x, p.y )/6.2832;
	float dist		= length(p);
	vec3 coord		= vec3( angle, dist, time * 0.1 );
	
	float newTime1	= abs( snoise( coord + vec3( 0.0, -time * ( 0.35 + brightness * 0.001 ), time * 0.015 ), 15.0 ) );
	float newTime2	= abs( snoise( coord + vec3( 0.0, -time * ( 0.15 + brightness * 0.001 ), time * 0.015 ), 45.0 ) );	
	for( int i=1; i<=7; i++ ){
		float power = pow( 2.0, float(i + 1) );
		fVal1 += ( 0.5 / power ) * snoise( coord + vec3( 0.0, -time, time * 0.2 ), ( power * ( 10.0 ) * ( newTime1 + 1.0 ) ) );
		fVal2 += ( 0.5 / power ) * snoise( coord + vec3( 0.0, -time, time * 0.2 ), ( power * ( 25.0 ) * ( newTime2 + 1.0 ) ) );
	}
	
	float corona		= pow( fVal1 * max( 1.1 - fade, 0.0 ), 2.0 ) * 50.0;
	corona				+= pow( fVal2 * max( 1.1 - fade, 0.0 ), 2.0 ) * 50.0;
	corona				*= 1.2 - newTime1;
	vec3 sphereNormal 	= vec3( 0.0, 0.0, 1.0 );
	vec3 dir 			= vec3( 0.0 );
	vec3 center			= vec3( 0.5, 0.5, 1.0 );
	vec3 starSphere		= vec3( 0.0 );
	
	vec2 sp = -1.0 + 2.0 * uv;
	sp.x *= aspect;
	sp *= ( 2.0 - brightness );
  	float r = dot(sp,sp);
	float f = (1.0-sqrt(abs(1.0-r)))/(r) + brightness * 0.5;
	if( dist < radius ){
		corona			*= pow( dist * invRadius, 24.0 );
  		vec2 newUv;
 		newUv.x = sp.x*f;
  		newUv.y = sp.y*f;
		newUv += vec2( time, 0.0 );
		
		vec3 texSample 	= texture( tex, newUv ).rgb;
		float uOff		= ( texSample.g * brightness * 3.14 + time );
		vec2 starUV		= newUv + vec2( uOff, 0.0 );
		starSphere		= texture( tex, starUV ).rgb;
	}
	
	float starGlow	= min( max( 1.0 - dist * ( 1.0 - brightness ), 0.0 ), 1.0 );
	//fragColor.rgb	= vec3( r );
	FragColor.rgb	= vec3( f * ( 0.75 + brightness * 0.3 ) * orange ) + starSphere + corona * orange + starGlow * orangeRed;
	FragColor.a		= 1.0;
}

void main()
{
	
	const float gamma = 2.2;
	vec3 hdrColor = texture(hdrBuffer, TexCoords.xy).rgb;
	vec3 result = vec3(1.0) - exp(-hdrColor * exposure);    
    result = pow(result, vec3(1.0 / gamma));
    //color = vec4(result, 1.0f);
    
	vec2 longlat = vec2((atan(TexCoords.y, TexCoords.x) / 3.1415926 + 1.0) * 0.5,
                                  (asin(TexCoords.z) / 3.1415926 + 0.5));
    vec3 color = vec3(texture(tex, longlat));
    vec3 N = 2.0*vec3(texture(normalTex, longlat))-1.0;
    N = normalize(N);
    
	//vec3 N = normalize(fN);
	//vec3 H = normalize( L + E );
    //vec3 E = fE;
    
    float Kd = max(dot(fL, N), 0.0);
    float Ks = pow(max(dot(N, fH), 0.0), Shininess);
    
    vec3 ambient = color;
    vec3 diffuse = Kd  * color;
    vec3 specular = Ks * color;

    FragColor = vec4(ambient + diffuse + specular, 1.0);//;
    //FragColor.a = 1.0;
}