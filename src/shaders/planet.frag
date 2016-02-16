#version 410

in vec3 fN;
in vec3 fL;
in vec3 fE;
in vec3 fH;
in vec4 TexCoords;
out vec4 FragColor;

uniform vec4 AmbientProduct, DiffuseProduct, SpecularProduct;
uniform float Shininess;

uniform float g;
uniform float g2;

invariant in vec3 v3Direction;
in vec4 secondaryColor;
in vec4 color;

uniform sampler2D tex;
uniform sampler2D normalTex;

float getMiePhase(float fCos, float fCos2, float g, float g2)
{
	return 1.5 * ((1.0 - g2) / (2.0 + g2)) * (1.0 + fCos2) / pow(1.0 + g2 - 2.0 * g * fCos, 1.5);
}

float getRayleighPhase(float fCos2)
{
	return 0.75 + 0.75 * fCos2;
}

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

    FragColor = vec4(ambient + diffuse + specular, 1.0);
    //FragColor.a = 1.0;
}
