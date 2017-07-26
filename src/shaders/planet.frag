#version 410

in vec3 fN;
in vec3 fL;
in vec3 fE;
in vec3 fH;
in vec3 fV;
in vec4 TexCoords;
out vec4 FragColor;

uniform vec4 ambientProduct, diffuseProduct, specularProduct;
uniform float shininess;

uniform sampler2D tex;
uniform sampler2D normalTex;
uniform sampler2D depthMap;

float height_scale = 0.05;

vec2 parallaxMapping(vec2 coords, vec3 viewDir)
{
	float height = texture(depthMap, coords.xy).r;
	return coords.xy - fV.xy / fV.z * (height * height_scale);
}

void main()
{	
	vec2 longlat = vec2((atan(TexCoords.y, TexCoords.x) / 3.1415926 + 1.0) * 0.5,
                                  (asin(TexCoords.z) / 3.1415926 + 0.5));
    vec3 viewDir = normalize(fV-fE);
    vec2 D = parallaxMapping(longlat, viewDir);
    vec3 color = vec3(texture(tex, D));
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
	//FragColor = vec4(D, 0.0, 1.0);
}
