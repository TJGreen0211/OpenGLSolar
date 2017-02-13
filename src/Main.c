#include "Main.h"
#include <stdio.h>
#include <unistd.h>

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

GLuint skyboxShader, sunShader, planetShader, atmosphereShader, ringShader;
GLuint vertUIShader, fragUIShader, UIShader;
GLuint planetVAO, skyboxVAO, objectVAO;
GLuint shadowFBO, depthMap, rboDepth;
GLuint ModelView, projection, model, view;
mat4 p, v;

GLfloat deltaTime = 0.0f;
GLfloat deltaSpeed;
float deltaTimeOffset;
GLfloat lastFrame = 0.0f;
int stopRotation = 0;
int drawGUI = 0;


int numPlanets = 11;
vec3 translation;

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

GLuint initPlanet() {
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
	
	return planetVAO;
		
}

GLuint initPlanetRing() {
	createShader(&ringShader, "src/shaders/ring.vert",
		"src/shaders/ring.frag");
	
	GLuint vPosition, vNormal, vTexture;
	GLuint ringVBO, ringVAO;
	
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
    
    return ringVAO;
}


GLuint initAtmosphere() {
	createShader(&atmosphereShader, "src/shaders/atmosphere.vert",
		"src/shaders/atmosphere.frag");
	
	GLuint vPosition, vNormal;
	GLuint atmosphereVBO, atmosphereVAO;
    
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
	
	return atmosphereVAO;
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
	planetRing = createRing(1, 21.0*50.0, 50.0*12.0);
	planet = tetrahedron(5, &planet);
	//atmosphere = tetrahedron(5);
	//sun = tetrahedron(3);
	//atmosphere = tetrahedron(5);
	createShader(&sunShader, "src/shaders/sun.vert",
		"src/shaders/sun.frag");
    
    initFramebuffer();
	
    glEnable(GL_DEPTH_TEST);
}

void createPerspectiveMatrix()
{
	p = perspective(zoom*45.0, getWindowWidth()/getWindowHeight(), zNear, zFar);
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
	//v = multiplymat4(getViewRotation(), planetInstance[i].planetLocation);
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

void drawAtmosphere(GLuint atmoVAO, atmosphereParameters *atmosphereArray, planetParameters *planetInstance)
{
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
		
	glUseProgram(atmosphereShader);
	//setupLighting(atmosphereShader);
	
	
	for(int i = 0; i < 11; i++)
    {	
    	if(planetInstance[i].createAtmosphere) {
			float scaleFactor = atmosphereArray[i].scaleFactor;
			float fOuter = planetInstance[i].size*scaleFactor;
			float fInner = (planetInstance[i].size);
		
			mat4 modelmat = planetInstance[i].planetLocation;
			mat4 tv = transposemat4(multiplymat4(modelmat, getViewPosition()));
			vec4 camMult = {-tv.m[3][0], -tv.m[3][1], -tv.m[3][2], -tv.m[3][3]};
			vec4 camPosition = multiplymat4vec4(tv, camMult);
		
			mat4 m = multiplymat4(modelmat, scale(planetInstance[i].size*scaleFactor));
			vec3 C_R = atmosphereArray[i].C_R;//{0.3, 0.7, 1.0};
			float E = atmosphereArray[i].E;//14.3;
		
			glUniform1f(glGetUniformLocation(atmosphereShader, "fInnerRadius"), fInner);
			glUniform1f(glGetUniformLocation(atmosphereShader, "fOuterRadius"), fOuter);
			glUniform3f(glGetUniformLocation(atmosphereShader, "camPosition"), camPosition.x, camPosition.y, camPosition.z);
			glUniform3f(glGetUniformLocation(atmosphereShader, "C_R"), C_R.x, C_R.y, C_R.z);
			glUniform1f(glGetUniformLocation(atmosphereShader, "E"), E);
			glUniform1f(glGetUniformLocation(atmosphereShader, "time"), glfwGetTime());

			initMVP(atmosphereShader, m, v);
		
			glBindVertexArray (atmoVAO);
			glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber);
			glBindVertexArray(0);
		}
    }
    glDisable(GL_BLEND);
}

void drawMoon(float theta, GLuint moonVAO, moonParameters *moonArray, planetParameters *planetInstance)
{
	glUseProgram(planetShader);
	setupLighting(planetShader);
	int c = 0;
	for(int i = 0; i < 11; i++)
	{
		for(int j = 0; j < planetInstance[i].numMoons; j++) {
			
	
			translation.x = (moonArray[c].radius+planetInstance[i].size) * cos(theta/moonArray[c].orbit);
			translation.y = 0.0;
			translation.z = (moonArray[c].radius+planetInstance[i].size) * sin(theta/moonArray[c].orbit);
	
			mat4 m = multiplymat4(multiplymat4(multiplymat4(planetInstance[i].planetLocation,translatevec3(translation)), scale(moonArray[c].size)),rotateX(90.0));
	
			initMVP(planetShader, m, v);
	
			glBindVertexArray (moonVAO);
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, moonArray[c].texture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, planetInstance[2].normal);
			glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
			glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
			glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber );
			glBindVertexArray(0);
			c++;
		}
    }
}


planetParameters *drawPlanet(float theta, GLuint vao, planetParameters *planetInstance)
{   		
	glUseProgram(planetShader);
	setupLighting(planetShader);
	for(int i = 0; i < 11; i++)
	{	
		translation.x = planetInstance[i].radius * cos(theta/planetInstance[i].orbit);
		translation.y = 0.0;
		translation.z = planetInstance[i].radius * sin(theta/planetInstance[i].orbit);
		mat4 matT = multiplymat4(translatevec3(translation), scale(planetInstance[i].size));
		mat4 matR = multiplymat4(rotateY(theta/planetInstance[i].day), rotateX(planetInstance[i].axialTilt+45.0));
		mat4 m = multiplymat4(matT,matR);
		planetInstance[i].planetLocation = translatevec3(translation);
		
		mat4 modelmat = planetInstance[i].planetLocation;
		mat4 tv = transposemat4(multiplymat4(modelmat, getViewPosition()));
		vec4 camMult = {-tv.m[3][0], -tv.m[3][1], -tv.m[3][2], -tv.m[3][3]};
		vec4 camPosition = multiplymat4vec4(tv, camMult);

		initMVP(planetShader, m, v);
    	
    	glBindVertexArray (vao);
    	bindTexture(GL_TEXTURE0, planetInstance[i].texture);
    	bindTexture(GL_TEXTURE1, planetInstance[i].normal);
    	bindTexture(GL_TEXTURE2, planetInstance[i].displacement);
    	glUniform1i(glGetUniformLocation(planetShader, "tex"), 0);
    	glUniform1i(glGetUniformLocation(planetShader, "normalTex"), 1);
    	glUniform1i(glGetUniformLocation(planetShader, "depthMap"), 2);
    	glUniform3f(glGetUniformLocation(atmosphereShader, "camPosition"), camPosition.x, camPosition.y, camPosition.z);
    	glDrawArrays( GL_TRIANGLES, 0, planet.vertexNumber);
    	glBindVertexArray(0);
    }
    
    return planetInstance;
}

void drawPlanetRing(GLuint vao, ringParameters *ringArray, planetParameters *planetInstance) 
{   
	glUseProgram(ringShader);
	setupLighting(ringShader);
	
	int c = 0;
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	
	for(int i = 0; i < 11; i++)
	{	
		if(planetInstance[i].createRing) {
			glDisable(GL_CULL_FACE);
			mat4 modelmat = planetInstance[i].planetLocation;
			mat4 m = multiplymat4(multiplymat4(modelmat, rotateX(planetInstance[i].axialTilt+45.0)),scale(1.0));

			initMVP(ringShader, m, v);
	
			glBindVertexArray (vao);
			bindTexture(GL_TEXTURE0, ringArray[c].texture);
			glUniform1i(glGetUniformLocation(ringShader, "tex"), 0);
			glDrawArrays( GL_TRIANGLES, 0, planetRing.vertexNumber);
			glBindVertexArray(0);
			glEnable(GL_CULL_FACE);
			c++;
			
		}
	}
	glDisable(GL_BLEND);
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

int initCubemap()
{
	char *cubemapArray[] = {
		"include/textures/skybox1/right.png",
		"include/textures/skybox1/left.png",
		"include/textures/skybox1/top.png",
		"include/textures/skybox1/bottom.png",
		"include/textures/skybox1/back.png",
		"include/textures/skybox1/front.png"
	};
	return loadCubemap(cubemapArray);
}

void initializePlanetButtons()
{
	button1 = initButton(0.0, 800.0, 100.0, &button1, "include/textures/startButton.png");
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
	//initWindow();
	
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
	
	planetParameters planetArray[11];
	moonParameters moonArray[21];
	atmosphereParameters atmosphereArray[11];
	ringParameters ringArray[2];
	
	*planetArray = *planetBuilder(planetArray);
	*moonArray = *moonBuilder(moonArray);
	*atmosphereArray = *atmosphereBuilder(atmosphereArray);
	*ringArray = *ringBuilder(ringArray);
	
	init();
	GLuint pVAO = initPlanet();
	initSkybox();
	initObject();
	GLuint atmoVAO = initAtmosphere();
	initGui();
	initButtons();
	initPlanetUI();
	GLuint ringVAO = initPlanetRing();
	createPerspectiveMatrix();
	
	initializePlanetButtons();
	
	glEnable(GL_CULL_FACE);
	glEnable(GL_MULTISAMPLE);
	glCullFace(GL_BACK);
	//glfwSwapInterval(0);
	
	float fpsFrames= 0.0;
	float lastTime = 0.0;
	while(!glfwWindowShouldClose(window))
	{
		GLfloat currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        v = getViewMatrix();
        
        fpsFrames++;
		if(currentFrame - lastTime >= 1.0)
		{
			//printf("%f\n", fpsFrames);
			fpsFrames = 0.0;
			lastTime += 1.0;
		}
        
		glfwPollEvents();
		doMovement();
		
		glfwPollEvents();
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, getWindowWidth(), getWindowHeight());

		drawSkybox(skyboxTexture);
		drawSun();
		*planetArray = *drawPlanet(theta, planetVAO, planetArray);
		drawPlanetRing(ringVAO, ringArray, planetArray);
		drawMoon(theta, planetVAO, moonArray, planetArray);
    	drawAtmosphere(atmoVAO, atmosphereArray, planetArray);
		drawObj();
		
		drawButton(button1);
		//drawPlanetButtons();
		
		if(stopRotation == 0){
			theta += 0.0005;
		}
		if(drawGUI) {
			drawPlanetUI(glfwGetTime());
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

void checkButtonPress(imgButton b, float xMouse, float yMouse)
{ 
	if(mousePosX <= b.xPos && mousePosX >= b.xPos+b.size && mousePosY >= b.yPos && mousePosY <= b.yPos-b.size)
	{
		if(stopRotation == 0) {
			stopRotation = 1;
			//buttonState(0);
		}
		else {
			stopRotation = 0;
			//buttonState(1);
		}
	}
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		checkButtonPress(button1, mousePosX, mousePosY);
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	zoom = processMouseScroll(yoffset, zoom);
}
