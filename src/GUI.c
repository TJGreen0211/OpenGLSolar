#include "GUI.h"
#include <stdio.h>

GLuint UIShader, UITexture;
GLuint UIvao, UIvbo;

void buttonActionListener()
{
}

float *changeCoordinates(float posx, float posy, float size, float array[]) {
	float aspect = 800.0/1400.0;
	
	float oglCoordx = ((posx/1400.0)*2.0 - 1.0);
	float oglCoordy = (posy/800.0)*2.0 - 1.0;
	
	float oglpx = ((posx-size)/1400.0)*2.0-1.0;
	float oglpy = (((posy-size)/800.0)*2.0-1.0);
	
	printf("%f, %f, %f, %f, %f\n\n", oglCoordx, oglCoordy, oglpx, oglpy, aspect);
	
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

void attachGUIShaders()
{
	createShader(&UIShader, "src/shaders/UI.vert",
		"src/shaders/UI.frag");
}

void initButton(float topx, float topy, float size) {
	float UIVertices[30] = {0};
	*UIVertices = *changeCoordinates(topx, topy, size, UIVertices);
	attachGUIShaders();
	
	glGenVertexArrays(1, &UIvao);
	glBindVertexArray(UIvao);
	glGenBuffers(1, &UIvbo);
	glBindBuffer(GL_ARRAY_BUFFER, UIvbo);
	
	glBufferData(GL_ARRAY_BUFFER, sizeof(UIVertices), UIVertices, GL_STATIC_DRAW);
	
	GLuint vPosition = glGetAttribLocation(UIShader, "vPosition");
	glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(vPosition);
	
	GLuint texCoord = glGetAttribLocation(UIShader, "texCoord");
	glVertexAttribPointer(texCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), BUFFER_OFFSET(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(texCoord);
	
	glBindVertexArray(0);
}



void drawUI()
{	
	UITexture = loadTexture("include/textures/button.png");
	glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, UITexture);
    glUniform1i(glGetUniformLocation(UIShader, "tex"), 0);
	
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDepthMask(GL_FALSE);
	glUseProgram(UIShader);
	glBindVertexArray(UIvao);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glDepthMask(GL_TRUE);
}

imgButton drawButton(float topx, float topy, float size)
{
	imgButton b;
	b.xTopRight = topx;
	b.xTopLeft = topx-size;
	b.yTopRight = 800-topy;
	b.yBottomRight = 800-(topy-size);
	initButton(topx, topy, size);
	
	return b;
}