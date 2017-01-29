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
	int createAtmosphere;
	int createRing;
	GLuint texture;
	GLuint normal;
	GLuint displacement;
	mat4 planetLocation;
	
} planetParameters;

typedef struct atmosphereParameters {
	float scaleFactor;
	float E;
	vec3 C_R;
} atmosphereParameters;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
GLuint loadShader(char *vertex_path, char *fragement_path);

int mousePosX, mousePosY;
imgButton button1, mercuryButton, venusButton, earthButton, marsButton, 
	jupiterButton, saturnButton, uranusButton, neptuneButton;
	
const float WIDTH = 1400, HEIGHT = 800;
float ASPECT = WIDTH/HEIGHT;

struct sphere planet;
struct sphere sun, atmosphere;
struct ring planetRing;
obj object;
int keys;
int actionPress;

float zNear = 0.5, zFar = 100000.0;
float theta = 0.0;
float zoom = 1;

GLuint sunTexture, ringTexture;
GLuint sunNormal;
	
GLuint moonTexture;
GLuint skyboxShader, sunShader, planetShader, atmosphereShader, ringShader;
GLuint vertUIShader, fragUIShader, UIShader;
GLuint planetVAO, skyboxVAO, objectVAO, atmosphereVAO, ringVAO;
GLuint shadowFBO, depthMap, rboDepth;
GLuint ModelView, projection, model, view;
mat4 p, v;

GLfloat deltaTime = 0.0f;
GLfloat deltaSpeed;
float deltaTimeOffset;
GLfloat lastFrame = 0.0f;
int stopRotation = 0;


int numPlanets = 11;
planetParameters planetInstanceArray[11];
atmosphereParameters atmosphereArray[11];
mat4 modelMatrices[11];
mat4 planetTransform[11];
float orbitSpeedArray[11] = {0};
float rotationSpeedArray[11] = {0};
float planetScaleMult = 50.0;
float planetRadMult = 2000.0;
vec3 translation;

float getScreenWidth()
{
	return HEIGHT;
}

float getScreenHeight()
{
	return WIDTH;
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

int *loadPlanetDisplacementMap(int planetTexArray[])
{
	char *textureStrings[] = {"include/textures/Planets/mercuryDisplacement.png", 
		"include/textures/Planets/venusDisplacement.png", 
		"include/textures/Planets/earthDisplacement.png",
		"include/textures/Planets/marsDisplacement.png",
		"include/textures/Planets/ceresDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/neptuneDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png",
		"include/textures/Planets/jupiterDisplacement.png"};
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
	
	int planetDisplacementArray[numTextures];
	*planetDisplacementArray = *loadPlanetDisplacementMap(planetDisplacementArray);
	
	mercury.radius = 0.4*planetRadMult; mercury.size = 0.38*planetScaleMult; mercury.orbit = 0.240846; mercury.axialTilt = 0.034; mercury.day = 58.646; mercury.createAtmosphere = 0; mercury.createRing = 0; 
	mercury.texture = planetTexArray[0]; mercury.normal = planetNormArray[0]; mercury.displacement = planetDisplacementArray[0];
	venus.radius = 0.7*planetRadMult; venus.size = 0.9499*planetScaleMult; venus.orbit = 0.615198; venus.axialTilt = 177.36; venus.day = 243.025; venus.createAtmosphere = 1; venus.createRing = 0; 
	venus.texture = planetTexArray[1]; venus.normal = planetNormArray[1]; venus.displacement = planetDisplacementArray[1];
	earth.radius = 1.0*planetRadMult; earth.size = 1.0*planetScaleMult; earth.orbit = 1.0; earth.axialTilt = 23.4392811; earth.day = 1; earth.createAtmosphere = 1; earth.createRing = 0; 
	earth.texture = planetTexArray[2]; earth.normal = planetNormArray[2]; earth.displacement = planetDisplacementArray[2];
	mars.radius = 1.5*planetRadMult; mars.size = 0.533*planetScaleMult; mars.orbit = 1.88; mars.axialTilt = 25.19; mars.day = 1.025957; mars.createAtmosphere = 1; mars.createRing = 0; 
	mars.texture = planetTexArray[3]; mars.normal = planetNormArray[3]; mars.displacement = planetDisplacementArray[3];
	ceres.radius = 2.77*planetRadMult; ceres.size = 0.07*planetScaleMult; ceres.orbit = 4.6; ceres.axialTilt = 4; ceres.day = 0.3781; ceres.createAtmosphere = 1; ceres.createRing = 0; 
	ceres.texture = planetTexArray[4]; ceres.normal = planetNormArray[4]; ceres.displacement = planetDisplacementArray[4];
	jupiter.radius = 5.2*planetRadMult; jupiter.size = 11.209*planetScaleMult; jupiter.orbit = 11.8618; jupiter.axialTilt = 3.13; jupiter.day = 0.413; jupiter.createAtmosphere = 0; jupiter.createRing = 0; 
	jupiter.texture = planetTexArray[5]; jupiter.normal = planetNormArray[5]; jupiter.displacement = planetDisplacementArray[5];
	saturn.radius = 9.5*planetRadMult; saturn.size = 9.45*planetScaleMult; saturn.orbit = 29.4571; saturn.axialTilt = 26.73; saturn.day = 0.439583; saturn.createAtmosphere = 0; saturn.createRing = 1; 
	saturn.texture = planetTexArray[6]; saturn.normal = planetNormArray[6]; saturn.displacement = planetDisplacementArray[6];
	uranus.radius = 19.2*planetRadMult; uranus.size = 4.007*planetScaleMult; uranus.orbit = 84.0205; uranus.axialTilt = 97.77; uranus.day = 0.71833; uranus.createAtmosphere = 0; uranus.createRing = 1; 
	uranus.texture = planetTexArray[7]; uranus.normal = planetNormArray[7]; uranus.displacement = planetDisplacementArray[7];
	neptune.radius = 30.1*planetRadMult; neptune.size = 3.883*planetScaleMult; neptune.orbit = 164.8; neptune.axialTilt = 28.32; neptune.day = 0.6713; neptune.createAtmosphere = 0; neptune.createRing = 0; 
	neptune.texture = planetTexArray[8]; neptune.normal = planetNormArray[8]; neptune.displacement = planetDisplacementArray[8];
	pluto.radius = 39*planetRadMult; pluto.size = 0.18*planetScaleMult; pluto.orbit = 248; pluto.axialTilt = 119.591; pluto.day = 6.387230; pluto.createAtmosphere = 1; pluto.createRing = 0; 
	pluto.texture = planetTexArray[9]; pluto.normal = planetNormArray[9]; pluto.displacement = planetDisplacementArray[9];
	eris.radius = 68*planetRadMult; eris.size = 0.182*planetScaleMult; eris.orbit = 558.04; eris.axialTilt = 61.45; eris.day = 1.079167 ; eris.createAtmosphere = 0; eris.createRing = 0; 
	eris.texture = planetTexArray[10]; eris.normal = planetNormArray[10]; eris.displacement = planetDisplacementArray[10];
	
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

}

void atmosphereBuilder() {
	atmosphereParameters mercury, venus, earth, mars, eris, jupiter, saturn, uranus, neptune, pluto, ceres;
	mercury.scaleFactor = 1.000;mercury.E = 14.3; 	mercury.C_R.x = 0.3; 	mercury.C_R.y = 0.7; 	mercury.C_R.z = 1.0;
	venus.scaleFactor = 1.3008; venus.E = 14.3; 	venus.C_R.x = 0.3; 	venus.C_R.y = 0.7; 	venus.C_R.z = 1.0;
	earth.scaleFactor = 1.0212; earth.E = 14.3; 	earth.C_R.x = 0.3; 	earth.C_R.y = 0.7; 	earth.C_R.z = 1.0;
	mars.scaleFactor = 1.0210; 	mars.E = 14.3; 		mars.C_R.x = 0.3; 	mars.C_R.y = 0.7; 	mars.C_R.z = 1.0;
	eris.scaleFactor = 1.001; 	eris.E = 14.3; 		eris.C_R.x = 0.3; 	eris.C_R.y = 0.7; 	eris.C_R.z = 1.0;
	jupiter.scaleFactor = 1.1448;jupiter.E = 14.3; 	jupiter.C_R.x = 0.3; 	jupiter.C_R.y = 0.7; 	jupiter.C_R.z = 1.0;
	saturn.scaleFactor = 0.0; 	saturn.E = 14.3; 	saturn.C_R.x = 0.3; 	saturn.C_R.y = 0.7; 	saturn.C_R.z = 1.0;
	uranus.scaleFactor = 0.0; 	uranus.E = 14.3; 	uranus.C_R.x = 0.3; 	uranus.C_R.y = 0.7; 	uranus.C_R.z = 1.0;
	neptune.scaleFactor = 0.0; 	neptune.E = 14.3; 	neptune.C_R.x = 0.3; 	neptune.C_R.y = 0.7; 	neptune.C_R.z = 1.0;
	pluto.scaleFactor = 3.5000; pluto.E = 14.3; 	pluto.C_R.x = 0.3; 	pluto.C_R.y = 0.7; 	pluto.C_R.z = 1.0;
	ceres.scaleFactor = 0.0; 	ceres.E = 14.3;		ceres.C_R.x = 0.3; 	ceres.C_R.y = 0.7; 	ceres.C_R.z = 1.0;
	
	atmosphereArray[0] = mercury;
	atmosphereArray[1] = venus;
	atmosphereArray[2] = earth;
	atmosphereArray[3] = mars;
	atmosphereArray[4] = ceres;
	atmosphereArray[5] = jupiter;
	atmosphereArray[6] = saturn;
	atmosphereArray[7] = uranus;
	atmosphereArray[8] = neptune;
	atmosphereArray[9] = pluto;
	atmosphereArray[10] = eris;
	
}

void moonBuilder()
{
	moonTexture = loadTexture("include/textures/Planets/moon.jpg");
}

vec3 *generateTangents(int vertexNumber, vec3 *points, vec3 *tangent)
{	
	vec3 edge1, edge2, deltaUV1, deltaUV2;
	
	for(int i = 0; i < vertexNumber; i+=3)
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

/*void *generateTexCoords()
{
	vec2 temp[vertexNumber];
	for(int i = 0; i < vertexNumber; i++)
	{
		temp[i].x = atan2(points[i].y, points[i].x) / 3.1415926 + 1.0 * 0.5;
        temp[i].y = asin(points[i].z) / 3.1415926 + 0.5;
	}
}
*/

void initObject() {
	GLuint vPosition, vNormal;
	GLuint objectVBO;

	object = ObjLoadModel("include/obj/torus.obj");
	
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
}

void initPlanet() {
	GLuint vPosition, vNormal, vTangent;
	GLuint planetVBO;

	createShader(&planetShader, "src/shaders/planet.vert",
		"src/shaders/planet.frag");
	
	vec3 tangent[planet.vertexNumber];
	*tangent = *generateTangents(planet.vertexNumber, planet.points, tangent);
	vec3 vna[planet.vertexNumber];
	*vna = *generateSmoothNormals(planet.vertexNumber, vna, planet.points, planet.normals);
		
	glGenVertexArrays(1, &planetVAO);
	glBindVertexArray(planetVAO);
	glGenBuffers(1, &planetVBO);
	glBindBuffer(GL_ARRAY_BUFFER, planetVBO);
	glBufferData(GL_ARRAY_BUFFER, planet.size + planet.nsize + sizeof(tangent), NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0,	planet.size, planet.points);
	glBufferSubData(GL_ARRAY_BUFFER, planet.size, planet.nsize, vna);	
	glBufferSubData(GL_ARRAY_BUFFER, planet.size+planet.nsize, sizeof(tangent),	tangent);
	
	vPosition = glGetAttribLocation(planetShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));	
    
    vNormal = glGetAttribLocation(planetShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size));
    
    vTangent = glGetAttribLocation(planetShader, "vTangent");
    glEnableVertexAttribArray(vTangent);
    glVertexAttribPointer(vTangent, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size+planet.nsize));
    
	glBindVertexArray(0);
		
}

void initPlanetRing() {
	createShader(&ringShader, "src/shaders/ring.vert",
		"src/shaders/ring.frag");
	
	GLuint vPosition, vNormal, vTexture;
	GLuint ringVBO;
	
	glGenVertexArrays(1, &ringVAO);
	glBindVertexArray(ringVAO);
	glGenBuffers(1, &ringVBO);
	glBindBuffer(GL_ARRAY_BUFFER, ringVBO);
	glBufferData(GL_ARRAY_BUFFER, planetRing.size + planetRing.nsize + planetRing.texsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0,	planetRing.size, planetRing.points);
	glBufferSubData(GL_ARRAY_BUFFER, planetRing.size, planetRing.nsize, planetRing.normals);
	glBufferSubData(GL_ARRAY_BUFFER, planetRing.size+planetRing.nsize, planetRing.texsize, planetRing.texCoords);
	
	vPosition = glGetAttribLocation(ringShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));	
    
    vNormal = glGetAttribLocation(ringShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planetRing.size));
    
    vTexture = glGetAttribLocation(ringShader, "vTexture");
    glEnableVertexAttribArray(vTexture);
    glVertexAttribPointer(vTexture, 2, GL_FLOAT, GL_FALSE, 2*sizeof(GLfloat), BUFFER_OFFSET(planetRing.size+planetRing.nsize));
    glBindVertexArray(0);
    
    for(int i = 0; i < planetRing.vertexNumber; i++) {
    	//printf("%f, %f\n", planetRing.texCoords[i].x, planetRing.texCoords[i].y);
    }
    //printf("Points: %d, Normal: %d, Tex: %d", planetRing.size, planetRing.nsize, planetRing.texsize);
}


void initAtmosphere() {
	createShader(&atmosphereShader, "src/shaders/atmosphere.vert",
		"src/shaders/atmosphere.frag");
	
	GLuint vPosition, vNormal;
	GLuint atmosphereVBO;
    
    glGenVertexArrays(1, &atmosphereVAO);
	glBindVertexArray(atmosphereVAO);
	glGenBuffers(1, &atmosphereVBO);
	glBindBuffer(GL_ARRAY_BUFFER, atmosphereVBO);
	glBufferData(GL_ARRAY_BUFFER, planet.size + planet.nsize, NULL, GL_STATIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, planet.size, planet.points);
	glBufferSubData(GL_ARRAY_BUFFER, planet.size, planet.nsize, planet.normals);
	vPosition = glGetAttribLocation(atmosphereShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(0));	
    
    vNormal = glGetAttribLocation(atmosphereShader, "vNormal");
    glEnableVertexAttribArray(vNormal);
    glVertexAttribPointer(vNormal, 3, GL_FLOAT, GL_FALSE, 3*sizeof(GLfloat), BUFFER_OFFSET(planet.size));
	glBindVertexArray(0);
}

void initSkybox() {
	GLuint vPosition;
	GLuint skyboxVBO;
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
    
    createShader(&skyboxShader, "src/shaders/skybox.vert",
		"src/shaders/skybox.frag");
    
	glGenVertexArrays(1, &skyboxVAO);
	glBindVertexArray(skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
    vPosition = glGetAttribLocation(skyboxShader, "vPosition");
    glEnableVertexAttribArray(vPosition);
    glVertexAttribPointer(vPosition, 3, GL_FLOAT , GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glBindVertexArray(0);
}

void initFramebuffer() {
	int shadowWidth = 1024;
	int shadowHeight = 1024;
	glGenFramebuffers(1, &shadowFBO);
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, shadowWidth, shadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    //glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        printf("Framebuffer not complete!\n");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void init()
{
	planetRing = createRing(1, 150.0, 200.0);
	planet = tetrahedron(5);
	//atmosphere = tetrahedron(5);
	//sun = tetrahedron(3);
	//atmosphere = tetrahedron(5);
	createShader(&sunShader, "src/shaders/sun.vert",
		"src/shaders/sun.frag");
    
    initFramebuffer();
    initPlanet();
	initSkybox();
	initObject();
	initAtmosphere();
	initPlanetRing();
	
    glEnable(GL_DEPTH_TEST);
}

void createPerspectiveMatrix()
{
	p = perspective(zoom*45.0, ASPECT, zNear, zFar);
}

void setupLighting(int program)
{	
    vec4 light_ambient = {0.2, 0.2, 0.2, 1.0};
    vec4 light_diffuse = {1.0, 1.0, 1.0, 1.0};
    vec4 light_specular = {1.0, 1.0, 1.0, 1.0};
    
    vec4 material_ambient = {0.2, 0.2, 1.0, 1.0};
    vec4 material_diffuse = {0.8, 0.8, 0.8, 1.0};
    vec4 material_specular = {0.5, 0.5, 0.5, 1.0};
    
    vec4 light_position = {0.0, 0.0, 1.0, 1.0}; 
    float material_shininess = 50.0f;
    
    vec4 ambient_product = multiplyvec4(light_ambient, material_ambient);
    vec4 diffuse_product = multiplyvec4(light_diffuse, material_diffuse);
    vec4 specular_product = multiplyvec4(light_specular, material_specular);
    
    glUniform4fv( glGetUniformLocation(program, "ambientProduct"), 1, (float*)(&ambient_product) );
    glUniform4fv( glGetUniformLocation(program, "diffuseProduct"), 1, (float*)(&diffuse_product) );
    glUniform4fv( glGetUniformLocation(program, "specularProduct"), 1, (float*)(&specular_product) );
	glUniform4fv( glGetUniformLocation(program, "lightPos"), 1, (float*)(&light_position) );
	glUniform1f ( glGetUniformLocation(program, "shininess"), material_shininess );
}

void initMVP(int shader, mat4 m, mat4 v)
{
	glUniformMatrix4fv(glGetUniformLocation( shader, "projection" ), 1, GL_FALSE, &p.m[0][0]);
	glUniformMatrix4fv( glGetUniformLocation( shader, "model" ), 1, GL_FALSE, &m.m[0][0] );
	glUniformMatrix4fv( glGetUniformLocation( shader, "view" ), 1, GL_FALSE, &v.m[0][0] );
}

void drawSkybox(skyboxTexture)
{
	glUseProgram(skyboxShader);
	
	glDepthMask(GL_FALSE);
	glDepthFunc(GL_LEQUAL);
    
    mat4 mv = getViewRotation();
    
	glUniformMatrix4fv(glGetUniformLocation( skyboxShader, "projection" ), 1, GL_TRUE, &p.m[0][0]);  
    glUniformMatrix4fv(glGetUniformLocation( skyboxShader, "ModelView" ), 1, GL_TRUE, &mv.m[0][0] );
    
    glBindVertexArray (skyboxVAO);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(glGetUniformLocation(skyboxShader, "skybox"), 0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    
    glDepthMask(GL_TRUE);
}

void drawObj()
{
	glUseProgram(planetShader);
	//mat4 rx = multiplymat4(translate(0.0, 0.0, -15.0), rotationSpace());
	mat4 m = scale(100);//rx;//multiplymat4(rx, rotateY(90));
	
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
	
	mat4 a = translate(0.0, 0.0, 0.0); //333.0 Times
	mat4 rotation = rotateY(theta);
	mat4 b = multiplymat4(a, rotation);
	mat4 m = multiplymat4(b, scale(100.0));
	
	initMVP(sunShader, m, v);
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
    //glBindTexture(GL_TEXTURE_2D, colorBuffer);
    glUniform1f(glGetUniformLocation(sunShader, "exposure"), 1.0);
}

void drawAtmosphere()
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
		
	glUseProgram(atmosphereShader);
	//setupLighting(atmosphereShader);
	
	
	for(int i = 0; i < 11; i++)
    {	
    	if(planetInstanceArray[i].createAtmosphere) {
			float scaleFactor = atmosphereArray[i].scaleFactor;
			float fOuter = planetInstanceArray[i].size*scaleFactor;
			float fInner = (planetInstanceArray[i].size);
			//translation.x = planetInstanceArray[2].radius * cos(orbitSpeedArray[2]);
			//translation.y = 0.0;
			//translation.z = planetInstanceArray[2].radius * sin(orbitSpeedArray[2]);
		
			mat4 modelmat = planetTransform[i];
			mat4 tv = transposemat4(multiplymat4(modelmat, getViewPosition()));
			vec4 camMult = {-tv.m[3][0], -tv.m[3][1], -tv.m[3][2], -tv.m[3][3]};
			vec4 camPosition = multiplymat4vec4(tv, camMult);
		
			//mat4 m = multiplymat4(translatevec3(translation), scale());
			mat4 m = multiplymat4(modelmat, scale(planetInstanceArray[2].size*scaleFactor));
			vec3 C_R = {0.3, 0.7, 1.0};
			float E = 14.3;
		
			glUniform1f(glGetUniformLocation(atmosphereShader, "fInnerRadius"), fInner);
			glUniform1f(glGetUniformLocation(atmosphereShader, "fOuterRadius"), fOuter);
			glUniform3f(glGetUniformLocation(atmosphereShader, "camPosition"), camPosition.x, camPosition.y, camPosition.z);
			glUniform3f(glGetUniformLocation(atmosphereShader, "C_R"), C_R.x, C_R.y, C_R.z);
			glUniform1f(glGetUniformLocation(atmosphereShader, "E"), E);
			glUniform1f(glGetUniformLocation(atmosphereShader, "time"), glfwGetTime());

			initMVP(atmosphereShader, m, v);
		
			glBindVertexArray (atmosphereVAO);
			glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber);
			glBindVertexArray(0);
		}
    }
    glDisable(GL_BLEND);
}

void drawPlanet()
{   		
	//drawAtmosphere(translation, planetInstanceArray[2].size, 1.025);
	
	glUseProgram(planetShader);
	setupLighting(planetShader);
	for(int i = 0; i < 11; i++)
	{	
		translation.x = planetInstanceArray[i].radius * cos(orbitSpeedArray[i]);
		translation.y = 0.0;
		translation.z = planetInstanceArray[i].radius * sin(orbitSpeedArray[i]);
		mat4 matT = multiplymat4(translatevec3(translation), scale(planetInstanceArray[i].size));
		mat4 matR = multiplymat4(rotateY(rotationSpeedArray[i]), rotateX(planetInstanceArray[i].axialTilt+45.0));
		mat4 m = multiplymat4(matT,matR);
		planetTransform[i] = translatevec3(translation);
		
		mat4 modelmat = planetTransform[i];
		mat4 tv = transposemat4(multiplymat4(modelmat, getViewPosition()));
		vec4 camMult = {-tv.m[3][0], -tv.m[3][1], -tv.m[3][2], -tv.m[3][3]};
		vec4 camPosition = multiplymat4vec4(tv, camMult);

		initMVP(planetShader, m, v);
    	
    	glBindVertexArray (planetVAO);
    	bindTexture(GL_TEXTURE0, planetInstanceArray[i].texture);
    	bindTexture(GL_TEXTURE1, planetInstanceArray[i].normal);
    	bindTexture(GL_TEXTURE2, planetInstanceArray[i].displacement);
    	glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
    	glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
    	glUniform1i(glGetUniformLocation(planetShader, "depthMap"), 2);
    	glUniform3f(glGetUniformLocation(atmosphereShader, "camPosition"), camPosition.x, camPosition.y, camPosition.z);
    	glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber);
    	glBindVertexArray(0);
    }
}

void drawPlanetRing()
{   
	glUseProgram(ringShader);
	setupLighting(ringShader);
	
	for(int i = 0; i < 11; i++)
	{	
		if(planetInstanceArray[i].createRing) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			glDisable(GL_CULL_FACE);
			mat4 modelmat = planetTransform[i];
			mat4 m = multiplymat4(multiplymat4(modelmat, rotateX(planetInstanceArray[i].axialTilt+45.0)),scale(5.0));

			initMVP(ringShader, m, v);
	
			glBindVertexArray (ringVAO);
			bindTexture(GL_TEXTURE0, ringTexture);
			glUniform1i(glGetUniformLocation(ringShader, "tex"), 0);
			glDrawArrays( GL_TRIANGLES, 0, planetRing.vertexNumber);
			glBindVertexArray(0);
			glEnable(GL_CULL_FACE);
			
			glDisable(GL_BLEND);
		}
	}
}

void drawMoon()
{
	glUseProgram(planetShader);
	setupLighting(planetShader);
	
	translation.x = 400.0 * cos(orbitSpeedArray[2]*12.0);
	translation.y = 0.0;
	translation.z = 400.0 * sin(orbitSpeedArray[2]*12.0);
	
	mat4 m = multiplymat4(multiplymat4(planetTransform[2],translatevec3(translation)), scale(planetInstanceArray[2].size/4.0));
	
	initMVP(planetShader, m, v);
    
    glBindVertexArray (planetVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planetInstanceArray[2].texture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, planetInstanceArray[2].normal);
 	glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
    glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
    glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber );
    glBindVertexArray(0);
}

void doMovement()
{
	//printf("%f\n", deltaSpeed);
	
	if(actionPress == GLFW_PRESS) {
		deltaTimeOffset += 1;
		if(deltaTimeOffset > 200)
			deltaSpeed += 0.5;
	}
	else {
		deltaSpeed = 0;
		deltaTimeOffset = 0;
	}
		
	if(keys == GLFW_KEY_W && actionPress == GLFW_PRESS)
        processKeyboard(FORWARD, deltaTime, deltaSpeed);
    if(keys == GLFW_KEY_S && actionPress == GLFW_PRESS)
        processKeyboard(BACKWARD, deltaTime, deltaSpeed);
    if(keys == GLFW_KEY_A && actionPress == GLFW_PRESS)
        processKeyboard(LEFT, deltaTime, deltaSpeed);
    if(keys == GLFW_KEY_D && actionPress == GLFW_PRESS)
        processKeyboard(RIGHT, deltaTime, deltaSpeed);
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

void initializePlanetButtons()
{
	button1 = initButton(1300.0, 700.0, 100.0, &button1, "include/textures/buttonUp.png");
	mercuryButton = initButton(100.0, 100.0, 50.0, &mercuryButton, "include/textures/buttons/mercury.png");
	venusButton = initButton(200.0, 100.0, 50.0, &venusButton, "include/textures/buttons/venus.png");
	earthButton = initButton(300.0, 100.0, 50.0, &earthButton, "include/textures/buttons/earth.png");
	marsButton = initButton(400.0, 100.0, 50.0, &marsButton, "include/textures/buttons/mars.png");
	
	jupiterButton = initButton(500.0, 100.0, 50.0, &jupiterButton, "include/textures/buttons/jupiter.png");
	saturnButton = initButton(600.0, 100.0, 50.0, &saturnButton, "include/textures/buttons/saturn.png");
	uranusButton = initButton(700.0, 100.0, 50.0, &uranusButton, "include/textures/buttons/uranus.png");
	neptuneButton = initButton(800.0, 100.0, 50.0, &neptuneButton, "include/textures/buttons/neptune.png");
}

void drawPlanetButtons()
{
	drawButton(mercuryButton);
	drawButton(venusButton);
	drawButton(earthButton);
	drawButton(marsButton);
	drawButton(jupiterButton);
	drawButton(saturnButton);
	drawButton(uranusButton);
	drawButton(neptuneButton);
}

int main(int argc, char *argv[])
{
	chdir("/Users/tjgreen/Documents/OpenGL/Sol");
	
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
	ringTexture = loadTexture("include/textures/Planets/saturnRing.png");
	planetBuilder();
	atmosphereBuilder();
	init();
	createPerspectiveMatrix();
	
	initializePlanetButtons();
	
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
        v = getViewMatrix();
        
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
		drawPlanetRing();
		//glFrontFace(GL_CW);
		drawAtmosphere();
		//glFrontFace(GL_CCW);
		drawObj();
		drawMoon();
		
		//drawButton(button1);
		//drawPlanetButtons();
		
		if(stopRotation == 0){
			for(int i = 0; i < 11; i++)
			{
				orbitSpeedArray[i] += 0.001/planetInstanceArray[i].orbit;
			}
			for(int i = 0; i < 11; i++)
			{
				rotationSpeedArray[i] += 0.1/planetInstanceArray[i].day;
			}
			theta += 0.009;
		}
		
		glfwSwapBuffers(window);
	}
	
	//glDeleteVertexArrays(1, &planetVAO);
    //glDeleteBuffers(1, &planetVBO);
	
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

void checkButtonPress(imgButton b)
{
	if(mousePosX <= b.xTopRight && mousePosX >= b.xTopLeft && mousePosY >= b.yTopRight && mousePosY <= b.yBottomRight)
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

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		checkButtonPress(button1);
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	zoom = processMouseScroll(yoffset, zoom);
}
