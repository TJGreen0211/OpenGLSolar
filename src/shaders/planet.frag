#version 410

in vec3 fN;
in vec3 fL;
in vec3 fE;
in vec3 fH;
in vec4 TexCoords;
out vec4 FragColor;
in vec3 vc;

uniform vec4 ambientProduct, diffuseProduct, specularProduct;
uniform float shininess;

uniform sampler2D tex;
uniform sampler2D normalTex;
uniform sampler2D depthMap;

float height_scale = 0.5;

vec2 parallaxMapping(vec2 texCoords)
{
	float height = texture(depthMap, texCoords).r;
	vec2 p = fE.xy / fE.z * (height * height_scale);
	return texCoords - p;
}

void main()
{	
	vec2 longlat = vec2((atan(TexCoords.y, TexCoords.x) / 3.1415926 + 1.0) * 0.5,
                                  (asin(TexCoords.z) / 3.1415926 + 0.5));
    vec2 D = parallaxMapping(longlat);
    vec3 color = vec3(texture(tex, longlat));
    vec3 N = 2.0*vec3(texture(normalTex, D))-1.0;
    
    N = normalize(N);
    
    float Kd = max(dot(fL, N), 0.0);
    float Ks = pow(max(dot(N, fH), 0.0), shininess);
    
    vec3 ambient = ambientProduct.xyz * color;
    vec3 diffuse = Kd * diffuseProduct.xyz * color;
    vec3 specular = Ks * specularProduct.xyz;

    // discard the specular highlight if the light's behind the vertex
    if( dot(fL, N) < 0.0 ) {
		specular = vec3(0.0, 0.0, 0.0);
    }
    
    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
