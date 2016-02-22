#include "Main.h"
#include <stdio.h>
#include <unistd.h>

typedef struct moonParameters {
	float radius;
	GLuint texture;
	GLuint normal;
} moonParameters;

typedef struct planetParameters {
	float radius; 
	float size;
	float orbit;
	float axialTilt;
	float day;
	GLuint texture;
	GLuint normal;
	mat4 planetLocation;
	
} planetParameters;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
GLuint loadShader(char *vertex_path, char *fragement_path);

int mousePosX, mousePosY;
imgButton button1, button2;
	
const float WIDTH = 1400, HEIGHT = 800;
float ASPECT = WIDTH/HEIGHT;

sphere planet, sun;
obj object;
int keys;
int actionPress;

float zNear = 0.5, zFar = 100000.0;
float thetaY = 0.0;
float zoom = 1;

GLuint sunTexture;
GLuint sunNormal;
	
GLuint moonTexture;
GLuint skyboxShader, sunShader, planetShader;
GLuint vertUIShader, fragUIShader, UIShader;
GLuint vPosition, vNormal, vTangent, vBitangent;
GLuint planetVAO, planetVBO, skyboxVAO, skyboxVBO, objectVAO, objectVBO;
GLuint hdrFBO, colorBuffer, rboDepth;
GLuint ModelView, projection, model, view;
mat4 mv, p, m, v;

GLfloat deltaTime = 0.0f;
GLfloat lastFrame = 0.0f;
int stopRotation = 0;

typedef struct stack {
	mat4 matrix;
} stack;

stack smat;

planetParameters planetInstanceArray[11];
mat4 modelMatrices[11];
float orbitSpeedArray[11] = {0};
float rotationSpeedArray[11] = {0};
vec4 light_position = {0.0, 0.0, -400.0, 1.0}; 

float getScreenWidth()
{
	return HEIGHT;
}

float getScreenHeight()
{
	return WIDTH;
}

char* getCurrentDirectory()
{
	int maxPathLen = 4096;
	char *cwd_buffer = malloc(sizeof(char) * maxPathLen);
	char *cwd_result = getcwd(cwd_buffer, maxPathLen);
	return cwd_result;
}

int *loadPlanetTextures(int planetTexArray[])
{
	char *textureStrings[] = {"include/textures/Planets/mercury.jpg", 
		"include/textures/Planets/venus.png", 
		"include/textures/Planets/earth.jpg",
		"include/textures/Planets/mars.jpg",
		"include/textures/Planets/ceres.jpg",
		"include/textures/Planets/jupiter.jpg",
		"include/textures/Planets/saturn.png",
		"include/textures/Planets/uranus.jpg",
		"include/textures/Planets/neptune.jpg",
		"include/textures/Planets/pluto.png",
		"include/textures/Planets/eris.jpg"};
	int arraySize = sizeof(textureStrings)/sizeof(textureStrings[0]);
	for(int i = 0; i < arraySize; i++)
	{
		planetTexArray[i] = loadTexture(textureStrings[i]);
	}
	return planetTexArray;
}

int *loadPlanetNormals(int planetTexArray[])
{
	char *textureStrings[] = {"include/textures/Planets/mercuryNormal.png", 
		"include/textures/Planets/venusNormal.png", 
		"include/textures/Planets/earthNormal.jpg",
		"include/textures/Planets/marsNormal.png",
		"include/textures/Planets/ceresNormal.png",
		"include/textures/Planets/jupiterNormal.png",
		"include/textures/Planets/saturnNormal.png",
		"include/textures/Planets/uranusNormal.png",
		"include/textures/Planets/neptuneNormal.png",
		"include/textures/Planets/plutoNormal.png",
		"include/textures/Planets/erisNormal.png"};
	int arraySize = sizeof(textureStrings)/sizeof(textureStrings[0]);
	for(int i = 0; i < arraySize; i++)
	{
		planetTexArray[i] = loadTexture(textureStrings[i]);
	}
	return planetTexArray;
}

void planetBuilder()
{
	planetParameters mercury, venus, earth, mars, eris, jupiter, saturn, uranus, neptune, pluto, ceres;
	int numTextures = 11;
	int planetTexArray[numTextures];
	*planetTexArray = *loadPlanetTextures(planetTexArray);
	
	int planetNormArray[numTextures];
	*planetNormArray = *loadPlanetNormals(planetNormArray);	
	
	mercury.radius = 0.4; mercury.size = 0.38; mercury.orbit = 0.240846; mercury.axialTilt = 0.034; mercury.day = 58.646; mercury.texture = planetTexArray[0]; mercury.normal = planetNormArray[0];
	venus.radius = 0.7; venus.size = 0.9499; venus.orbit = 0.615198; venus.axialTilt = 177.36; venus.day = 243.025; venus.texture = planetTexArray[1]; venus.normal = planetNormArray[1];
	earth.radius = 1.0; earth.size = 1.0; earth.orbit = 1.0; earth.axialTilt = 23.4392811; earth.day = 1; earth.texture = planetTexArray[2]; earth.normal = planetNormArray[2];
	mars.radius = 1.5; mars.size = 0.533; mars.orbit = 1.88; mars.axialTilt = 25.19; mars.day = 1.025957; mars.texture = planetTexArray[3]; mars.normal = planetNormArray[3];
	ceres.radius = 2.77; ceres.size = 0.07; ceres.orbit = 4.6; ceres.axialTilt = 4; ceres.day = 0.3781; ceres.texture = planetTexArray[4]; ceres.normal = planetNormArray[4];
	jupiter.radius = 5.2; jupiter.size = 11.209; jupiter.orbit = 11.8618; jupiter.axialTilt = 3.13; jupiter.day = 0.413; jupiter.texture = planetTexArray[5]; jupiter.normal = planetNormArray[5];
	saturn.radius = 9.5; saturn.size = 9.45; saturn.orbit = 29.4571; saturn.axialTilt = 26.73; saturn.day = 0.439583; saturn.texture = planetTexArray[6]; saturn.normal = planetNormArray[6];
	uranus.radius = 19.2; uranus.size = 4.007; uranus.orbit = 84.0205; uranus.axialTilt = 97.77; uranus.day = 0.71833; uranus.texture = planetTexArray[7]; uranus.normal = planetNormArray[7];
	neptune.radius = 30.1; neptune.size = 3.883; neptune.orbit = 164.8; neptune.axialTilt = 28.32; neptune.day = 0.6713; neptune.texture = planetTexArray[8]; neptune.normal = planetNormArray[8];
	pluto.radius = 39; pluto.size = 0.18; pluto.orbit = 248; pluto.axialTilt = 119.591; pluto.day = 6.387230; pluto.texture = planetTexArray[9]; pluto.normal = planetNormArray[9];
	eris.radius = 68; eris.size = 0.182; eris.orbit = 558.04; eris.axialTilt = 61.45; eris.day = 1.079167 ; eris.texture = planetTexArray[10]; eris.normal = planetNormArray[10];
	
	planetInstanceArray[0] = mercury;
	planetInstanceArray[1] = venus;
	planetInstanceArray[2] = earth;
	planetInstanceArray[3] = mars;
	planetInstanceArray[4] = ceres;
	planetInstanceArray[5] = jupiter;
	planetInstanceArray[6] = saturn;
	planetInstanceArray[7] = uranus;
	planetInstanceArray[8] = neptune;
	planetInstanceArray[9] = pluto;
	planetInstanceArray[10] = eris;
	
	/*for(int i = 0; i < 11; i++)
	{
		mat4 a = multiplymat4(translate(planetInstanceArray[i].radius*1000+333, 0.0, 0.0), translatevec4(light_position));
		mat4 b = multiplymat4(translate(0.0, 0.0, -400.0), getViewMatrix());
		mat4 roty = rotateY(planetInstanceArray[i].orbit);
		mat4 rotxy = multiplymat4(b, roty);
		mat4 c = multiplymat4(rotxy, a);
		modelMatrices[i] = multiplymat4(c, scale(planetInstanceArray[i].size*100));
	}*/

}

void moonBuilder()
{
	moonTexture = loadTexture("include/textures/Planets/moon.jpg");
}

vec3 *generateTangents(int vertexNumber, vec3 *points, vec3 *tangent, vec3 *bitangent)
{
	vec2 temp[vertexNumber];
	
	vec3 edge1, edge2, deltaUV1, deltaUV2;
	
	for(int i = 0; i < vertexNumber; i++)
	{
		temp[i].x = atan2(points[i].y, points[i].x) / 3.1415926 + 1.0 * 0.5;
        temp[i].y = asin(points[i].z) / 3.1415926 + 0.5;
	}
	for(int i = 0; i < vertexNumber-3; i+=3)
	{
		edge1.x = points[i+1].x - points[i].x;
		edge1.y = points[i+1].y - points[i].y;
		edge1.z = points[i+1].z - points[i].z;
		edge2.x = points[i+2].x - points[i].x;
		edge2.y = points[i+2].y - points[i].y;
		edge2.z = points[i+2].z - points[i].z;
		
		deltaUV1.x = points[i+1].x - points[i].x;
		deltaUV1.y = points[i+1].y - points[i].y;
		deltaUV2.x = points[i+2].x - points[i].x;
		deltaUV2.y = points[i+2].y - points[i].y;
		
		float f = 1.0 / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
		tangent[i].x = f  * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
		tangent[i].y = f  * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
		tangent[i].z = f  * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
		tangent[i] = normalizevec3(tangent[i]);
		for(int j = i; j < i+3; j++)
			tangent[j] = tangent[i];
		
		//bitangent[i] = crossvec3(planet.normals[i], tangent[i]);
		bitangent[i].x = f  * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
		bitangent[i].y = f  * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
		bitangent[i].z = f  * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
		bitangent[i] = normalizevec3(bitangent[i]);
		for(int j = i; j < i+3; j++)
			bitangent[j] = bitangent[i];
		
	}	
	return tangent;	
}

vec3 *generateSmoothNormals(int vertexNumber, vec3 *vna, vec3 *pointArray, vec3 *normalArray)
{
	vec3 vn;
	
	for(int i = 0; i < vertexNumber; i++)
	{
		vec3 tempvn = {0.0, 0.0, 0.0};
		vn = pointArray[i];
		for(int j = 0; j < vertexNumber; j++)
		{
			if(vn.x == pointArray[j].x && vn.y == pointArray[j].y && vn.z == pointArray[j].z)
			{
				tempvn = plusequalvec3(tempvn, normalArray[j]);
			}
		}
		vna[i] = normalizevec3(tempvn);
	}
	return vna;
}

void generateBuffers()
{
	
}

void init()
{
	GLfloat skyboxVertices[] = {
        // Positions          
        -1.0f, 1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, -1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,

        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, -1.0f, 1.0f,
        -1.0f, -1.0f, 1.0f,

        -1.0f, 1.0f, -1.0f,
        1.0f, 1.0f, -1.0f,
        1.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, -1.0f,
        1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f, 1.0f,
        1.0f, -1.0f, 1.0f
    };  

	planet = tetrahedron(5);
	object = ObjLoadModel("include/obj/torus.obj");

	vec3 tangent[planet.vertexNumber];
	vec3 bitangent[planet.vertexNumber];
	*tangent = *generateTangents(planet.vertexNumber, planet.points, tangent, bitangent);
	
	vec3 vna[planet.vertexNumber];
	*vna = *generateSmoothNormals(planet.vertexNumber, vna, planet.points, planet.normals);
	
	createShader(&skyboxShader, "src/shaders/skybox.vert",
		"src/shaders/skybox.frag");
	createShader(&sunShader, "src/shaders/sun.vert",
		"src/shaders/sun.frag");
	createShader(&planetShader, "src/shaders/planet.vert",
		"src/shaders/planet.frag");
    
    
    glGenFramebuffers(1, &hdrFBO);
    glGenTextures(1, &colorBuffer);
    glBindTexture(GL_TEXTURE_2D, colorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, WIDTH, HEIGHT, 0, GL_RGB, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, WIDTH, HEIGHT);
    
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("Framebuffer not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glGenVertexArrays(1, &planetVAO);
	glBindVertexArray(planetVAO);
	glGenBuffers(1, &planetVBO);
	glBindBuffer(GL_ARRAY_BUFFER, planetVBO);
	glBufferData(GL_ARRAY_BUFFER, planet.size + planet.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, planet.size, planet.points);
	glBufferSubData(GL_ARRAY_BUFFER, planet.size, planet.nsize, vna);
	
	glBufferSubData(GL_ARRAY_BUFFER, planet.size+planet.nsize, sizeof(tangent), tangent);
	glBufferSubData(GL_ARRAY_BUFFER, planet.size+planet.nsize+sizeof(tangent), sizeof(tangent), tangent);
	
	vPosition = glGetAttribLocation(planetShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));	
    vNormal = glGetAttribLocation(planetShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size));
    
    vTangent = glGetAttribLocation(planetShader, "vTangent");
    glEnableVertexAttribArray(vTangent);
    glVertexAttribPointer(vTangent, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size+planet.nsize));
    
    vBitangent = glGetAttribLocation(planetShader, "vBitangent");
    glEnableVertexAttribArray(vBitangent);
    glVertexAttribPointer(vBitangent, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size+planet.nsize+sizeof(tangent)));
    
	glBindVertexArray(0);
	
	glGenVertexArrays(1, &skyboxVAO);
	glBindVertexArray(skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    vPosition = glGetAttribLocation(skyboxShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glBindVertexArray(0);
    
    glGenVertexArrays(1, &objectVAO);
	glBindVertexArray(objectVAO);
	glGenBuffers(1, &objectVBO);
	glBindBuffer(GL_ARRAY_BUFFER, objectVBO);
	glBufferData(GL_ARRAY_BUFFER, object.size + object.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, object.size, object.points);
	glBufferSubData(GL_ARRAY_BUFFER, object.size, object.nsize, object.normals);
	vPosition = glGetAttribLocation(sunShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));	
    
    vNormal = glGetAttribLocation(sunShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(object.size));
	glBindVertexArray(0);
	
    glEnable(GL_DEPTH_TEST);
}

void createPerspectiveMatrix()
{
	p = perspective(zoom*45.0, ASPECT, zNear, zFar);
}

mat4 rotationSpace()
{
	vec2 rotation = getCameraRotation();
    mat4 rx = rotateX(rotation.y);
	mat4 ry = rotateY(rotation.x);
	mat4 rxry = multiplymat4(rx, ry);
	return rxry;
}

void setupLighting(int prog)
{	
    vec4 light_ambient = {0.2, 0.2, 0.2, 1.0};
    vec4 light_diffuse = {1.0, 1.0, 1.0, 1.0};
    vec4 light_specular = {1.0, 1.0, 1.0, 1.0};
    
    vec4 material_ambient = {0.2, 0.2, 1.0, 1.0};
    vec4 material_diffuse = {0.8, 0.8, 0.8, 1.0};
    vec4 material_specular = {0.5, 0.5, 0.5, 1.0};
    float material_shininess = 25.0f;
    
    vec4 ambient_product = multiplyvec4(light_ambient, material_ambient);
    vec4 diffuse_product = multiplyvec4(light_diffuse, material_diffuse);
    vec4 specular_product = multiplyvec4(light_specular, material_specular);
    
    glUniform4fv( glGetUniformLocation(prog, "AmbientProduct"), 1, (float*)(&ambient_product) );
    glUniform4fv( glGetUniformLocation(prog, "DiffuseProduct"), 1, (float*)(&diffuse_product) );
    glUniform4fv( glGetUniformLocation(prog, "SpecularProduct"), 1, (float*)(&specular_product) );
	glUniform4fv( glGetUniformLocation(prog, "LightPosition"), 1, (float*)(&light_position) );
	glUniform1f( glGetUniformLocation(prog, "Shininess"), material_shininess );
}

void initMVP(int shader, mat4 m, mat4 v)
{
	glUniformMatrix4fv(glGetUniformLocation( shader, "projection" ), 1, GL_TRUE, &p.m[0][0]);
	glUniformMatrix4fv( glGetUniformLocation( shader, "model" ), 1, GL_TRUE, &m.m[0][0] );
	glUniformMatrix4fv( glGetUniformLocation( shader, "view" ), 1, GL_TRUE, &v.m[0][0] );
}

void drawSkybox(skyboxTexture)
{
	smat.matrix = mv;
	glUseProgram(skyboxShader);
	
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
    
    mv = rotationSpace();
    
	glUniformMatrix4fv(glGetUniformLocation( skyboxShader, "projection" ), 1, GL_TRUE, &p.m[0][0]);  
    glUniformMatrix4fv(glGetUniformLocation( skyboxShader, "ModelView" ), 1, GL_TRUE, &mv.m[0][0] );
    
    glBindVertexArray (skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    glDepthMask(GL_TRUE);
    mv = smat.matrix;
}

void drawObj()
{
	v = getViewMatrix();
	glUseProgram(planetShader);
	//mat4 rx = multiplymat4(translate(0.0, 0.0, -15.0), rotationSpace());
	m = multiplymat4(translatevec4(light_position), scale(100));//rx;//multiplymat4(rx, rotateY(90));
	
	initMVP(planetShader, m, v);
	
	glBindVertexArray (objectVAO);
	//glDrawArrays( GL_TRIANGLES, 0, object.vertexNumber );
    glBindVertexArray(0);
}

void bindTexture(GLuint activeTex, GLuint tex) {
    glActiveTexture(activeTex);
    glBindTexture(GL_TEXTURE_2D, tex);
}

void drawSun()
{   
	glUseProgram(sunShader);
	
	setupLighting(sunShader);
	
	//glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
	vec3 Position = getCameraPosition();
	
	v = getViewMatrix();
	
	mat4 a = translate(0.0, 0.0, -400.0); //333.0 Times
	mat4 rotation = rotateY(thetaY);
	mat4 b = multiplymat4(a, rotation);
	m = multiplymat4(b, scale(100.0));
	
	initMVP(sunShader, m, v);
	glUniform3f(glGetUniformLocation(sunShader, "cameraPos"), Position.x, Position.y, Position.z);
	glUniform1f(glGetUniformLocation(sunShader, "systemTime"), glfwGetTime());
    
    glBindVertexArray (planetVAO);
    bindTexture(GL_TEXTURE0, sunTexture);
    bindTexture(GL_TEXTURE1, sunNormal);
    glUniform1i(glGetUniformLocation(sunShader, "tex"), 0);
    glUniform1i(glGetUniformLocation(sunShader, "normalTex"), 1);
    
    glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber );
    glBindVertexArray(0);
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);  
    
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, colorBuffer);
    glUniform1f(glGetUniformLocation(sunShader, "exposure"), 1.0);
}

void drawPlanet()
{   
	glUseProgram(planetShader);
	setupLighting(planetShader);
	vec3 Position = getCameraPosition();
	v = getViewMatrix();
	
	for(int i = 0; i < 11; i++)
	{	
		mat4 rotation = multiplymat4(rotateY(rotationSpeedArray[i]), rotateX(planetInstanceArray[i].axialTilt+45));
		mat4 translation = multiplymat4(translate(planetInstanceArray[i].radius*1000, 0.0, 0.0), rotation);
		mat4 b = translate(0.0, 0.0, -400.0);
		mat4 roty = rotateY(orbitSpeedArray[i]);
		mat4 rotxy = multiplymat4(b, roty);
		mat4 c = multiplymat4(rotxy, translation);
		m = multiplymat4(c, scale(planetInstanceArray[i].size*100));
		planetInstanceArray[i].planetLocation = c;
	
		//mv = modelMatrices[i];
		initMVP(planetShader, m, v);
		glUniform3f(glGetUniformLocation(planetShader, "cameraPos"), Position.x, Position.y, Position.z);
    
    	glBindVertexArray (planetVAO);
    	bindTexture(GL_TEXTURE0, planetInstanceArray[i].texture);
    	bindTexture(GL_TEXTURE1, planetInstanceArray[i].normal);
    	glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
    	glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
    	glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber );
    	glBindVertexArray(0);
    }
    /*
    glUniform3f(glGetUniformLocation(planetShader, "v3CameraPos"), Position.x, Position.y, Position.z);
    glUniform3f(glGetUniformLocation(planetShader, "v3InvWavelength"), 1.0f / pow(0.650f, 4.0f), 1.0f / pow(0.570f, 4.0f), 1.0f / pow(0.475f, 4.0f));
    glUniform1f(glGetUniformLocation(planetShader, "fCameraHeight"), 10.0f+lengthvec3(getCameraPosition()));
    glUniform1f(glGetUniformLocation(planetShader, "fCameraHeight2"), (10.0f+lengthvec3(getCameraPosition())) * (10.0f+lengthvec3(getCameraPosition())));
    glUniform1f(glGetUniformLocation(planetShader, "fInnerRadius"), 10.0f);
    glUniform1f(glGetUniformLocation(planetShader, "fInnerRadius2"), 100.0f);
    glUniform1f(glGetUniformLocation(planetShader, "fOuterRadius"), 10.25f);
    glUniform1f(glGetUniformLocation(planetShader, "fOuterRadius2"), 10.25f*10.25f);
    
    glUniform1f(glGetUniformLocation(planetShader, "fKrESun"), 0.0025f * 20.0f);
    glUniform1f(glGetUniformLocation(planetShader, "fKmESun"), 0.0015f * 20.0f);
    glUniform1f(glGetUniformLocation(planetShader, "fKr4PI"), 0.0025f * 4.0f * 3.141592653f);
    glUniform1f(glGetUniformLocation(planetShader, "fKm4PI"), 0.0015f * 4.0f * 3.141592653f);
    glUniform1f(glGetUniformLocation(planetShader, "fScale"), 1.0f / 0.25f);
    
    glUniform1f(glGetUniformLocation(planetShader, "fScaleDepth"), 0.25f);
    glUniform1f(glGetUniformLocation(planetShader, "fScaleOverScaleDepth"), 4.0f / 0.25f);
    glUniform1f(glGetUniformLocation(planetShader, "g"), -0.990f);
    glUniform1f(glGetUniformLocation(planetShader, "g2"), -0.990f*-0.990f);
    
    
    //vec3 lightDirection = light_position*/
    
}

void drawMoon()
{
	glUseProgram(planetShader);
	setupLighting(planetShader);
	vec3 Position = getCameraPosition();
	v = getViewMatrix();
	
	mat4 rt = multiplymat4(rotateY(orbitSpeedArray[3]), translate(planetInstanceArray[3].radius*1000, 0.0, 0.0));
	m = multiplymat4(multiplymat4(rt, translate(10.0, 0.0, 0.0)), scale(100));
	
	//mv = modelMatrices[i];
	initMVP(planetShader, m, v);
	glUniform3f(glGetUniformLocation(planetShader, "cameraPos"), Position.x, Position.y, Position.z);
    
    glBindVertexArray (planetVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planetInstanceArray[2].texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, planetInstanceArray[2].normal);
 	glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
    glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
    //glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber );
    glBindVertexArray(0);
}

void doMovement()
{
	if(keys == GLFW_KEY_W && actionPress == GLFW_PRESS)
        processKeyboard(FORWARD, deltaTime);
    if(keys == GLFW_KEY_S && actionPress == GLFW_PRESS)
        processKeyboard(BACKWARD, deltaTime);
    if(keys == GLFW_KEY_A && actionPress == GLFW_PRESS)
        processKeyboard(LEFT, deltaTime);
    if(keys == GLFW_KEY_D && actionPress == GLFW_PRESS)
        processKeyboard(RIGHT, deltaTime);
}

GLFWwindow *setupGLFW() {
	glfwInit();
	glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "SolarSystem", NULL, NULL);
	glfwMakeContextCurrent(window);
	//glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_ENABLED);
	glfwSetKeyCallback(window, key_callback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	return window;
}

int initCubemap()
{
	char *cubemapArray[] = {
		"include/textures/skybox3/right.png",
		"include/textures/skybox3/left.png",
		"include/textures/skybox3/top.png",
		"include/textures/skybox3/bottom.png",
		"include/textures/skybox3/back.png",
		"include/textures/skybox3/front.png"
	};
	return loadCubemap(cubemapArray);
}

int main(int argc, char *argv[])
{
	chdir("/Users/tylergreen/Documents/Programming/Sol");
	
	GLFWwindow *window = setupGLFW();
	GLuint skyboxTexture = initCubemap();
	
	//GLFWwindow* window2 = glfwCreateWindow(500, 500, "SolarSystem", NULL, NULL);
	//glfwMakeContextCurrent(window2);
	
	/*Cross platform compatibility stuff uncomment if not on mac
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
  		printf(stderr, "Error: %s\n", glewGetErrorString(err));
	}*/
	
	sunTexture = loadTexture("include/textures/Planets/sun2.jpg");
	sunNormal = loadTexture("include/textures/Planets/sunNormal.png");
	planetBuilder();
	init();
	createPerspectiveMatrix();
	
	button1 = initButton(1300.0, 700.0, 100.0, button1, "include/textures/buttons/button.png");
	button2 = initButton(700.0, 700.0, 100.0, button2, "include/textures/buttonUp.png");
	
	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	glCullFace(GL_BACK);
	attachGUIShaders();
	
	float fpsFrames= 0;
	float lastTime = 0;
	while(!glfwWindowShouldClose(window))
	{
		GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        
        fpsFrames++;
		if(currentFrame - lastTime >= 1.0)
		{
			//printf("%f\n", 1000/fpsFrames);
			fpsFrames = 0;
			lastTime += 1.0;
		}
        
		glfwPollEvents();
		doMovement();
		
		glfwPollEvents();
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, WIDTH, HEIGHT);

		drawSkybox(skyboxTexture);
		drawSun();
		drawPlanet();
		drawObj();
		drawMoon();
		
		drawButton(button1);
		drawButton(button2);
		
		if(stopRotation == 0){
			for(int i = 0; i < 11; i++)
			{
				orbitSpeedArray[i] += 0.1/planetInstanceArray[i].orbit;
			}
			for(int i = 0; i < 11; i++)
			{
				rotationSpeedArray[i] += 0.1/planetInstanceArray[i].day;
			}
			thetaY += 0.1;
		}
		
		glfwSwapBuffers(window);
	}
	
	glDeleteVertexArrays(1, &planetVAO);
    glDeleteBuffers(1, &planetVBO);
	
	glfwTerminate();
	return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

	if(action == GLFW_PRESS)
    	actionPress = GLFW_PRESS;
    else if(action == GLFW_RELEASE)
    	actionPress = 0;
    	
    if (key == GLFW_KEY_W && action == GLFW_PRESS){
    	keys = GLFW_KEY_W;
    	doMovement();
    }
    if (key == GLFW_KEY_S && action == GLFW_PRESS){
    	keys = GLFW_KEY_S;
    	doMovement();
    }
    if (key == GLFW_KEY_A && action == GLFW_PRESS){
    	keys = GLFW_KEY_A;
    	doMovement();
    }
    if (key == GLFW_KEY_D && action == GLFW_PRESS){
    	keys = GLFW_KEY_D;
    	doMovement();
    }
    if(action == GLFW_RELEASE)
    	keys = 0;
    if (key == GLFW_KEY_R && action == GLFW_PRESS){
		if(stopRotation == 0)
			stopRotation = 1;
		else
			stopRotation = 0;
    }
}


void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
	if (state == GLFW_PRESS)
	{
		processMouseMovement(xpos, ypos);
	}
	mousePosX = xpos;
	mousePosY = ypos;
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && mousePosX <= button1.xTopRight && mousePosX >= button1.xTopLeft && mousePosY >= button1.yTopRight && mousePosY <= button1.yBottomRight)
	{
		if(stopRotation == 0) {
			stopRotation = 1;
			buttonState(0);
		}
		else {
			stopRotation = 0;
			buttonState(1);
		}
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	zoom = processMouseScroll(yoffset, zoom);
}
