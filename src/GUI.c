#include "GUI.h"
#include <stdio.h>

GLuint UIShader, UITexture;
GLuint UIvao, UIvbo;

void buttonState(int state)
{
	if(state == 1)
		UITexture = loadTexture("include/textures/buttonUp.png");
	else
		UITexture = loadTexture("include/textures/buttonDown.png");
}

void attachGUIShaders()
{
	createShader(&UIShader, "src/shaders/UI.vert",
		"src/shaders/UI.frag");
}

void drawUI(int textureID)
{	
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(UIShader, "tex"), 0);
	
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	
	glUseProgram(UIShader);
	glBindVertexArray(UIvao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);
	
	glBindVertexArray(0);
}

void drawButton(imgButton b) {
	glGenVertexArrays(1, &UIvao);
	glBindVertexArray(UIvao);
	glGenBuffers(1, &UIvbo);
	glBindBuffer(GL_ARRAY_BUFFER, UIvbo);
	
	glBufferData(GL_ARRAY_BUFFER, b.size, b.pointArray, GL_STATIC_DRAW);
	
	GLuint vPosition = glGetAttribLocation(UIShader, "vPosition");
	glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(vPosition);
	
	GLuint texCoord = glGetAttribLocation(UIShader, "texCoord");
	glVertexAttribPointer(texCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), BUFFER_OFFSET(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(texCoord);
	
	drawUI(b.texture);
	
	glBindVertexArray(0);
}

float *changeCoordinates(float posx, float posy, float size, float array[]) {
	float oglCoordx = ((posx/1400.0)*2.0 - 1.0);
	float oglCoordy = (posy/800.0)*2.0 - 1.0;
	
	float oglpx = ((posx-size)/1400.0)*2.0-1.0;
	float oglpy = (((posy-size)/800.0)*2.0-1.0);
	
	float a1[] = {
		oglCoordx,  oglCoordy, 0.0f, 	1.0f, 1.0f, // Top Right
    	oglCoordx, 	oglpy, 0.0f, 1.0f, 0.0f, // Bottom Right
    	oglpx,  oglCoordy, 0.0f,	0.0f, 1.0f,  // Top Left 
    
    	oglCoordx, oglpy, 0.0f, 1.0f, 0.0f, // Bottom Right
    	oglpx, oglpy, 0.0f, 0.0f, 0.0f, // Bottom Left
    	oglpx,  oglCoordy, 0.0f,	0.0f, 1.0f,  // Top Left 
	};
	
	for(int i = 0; i < 30; i++)
		array[i] = a1[i];
	
	return array;
}

imgButton initButton(float topx, float topy, float size, imgButton *b, char* texture)
{
	b->texture = loadTexture(texture);
	b->xTopRight = topx;
	b->xTopLeft = topx-size;
	b->yTopRight = 800-topy;
	b->yBottomRight = 800-(topy-size);
	b->size = 30*sizeof(float);
	
	float UIVertices[30] = {0};
	*UIVertices = *changeCoordinates(topx, topy, size, UIVertices);
	b->pointArray = malloc(b->size);
	for(int i = 0; i < 30; i++)
	{
		b->pointArray[i] = UIVertices[i];
	}
	
	return *b;
}