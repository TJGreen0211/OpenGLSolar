#include "GUI.h"
#include <stdio.h>

GLuint GUIShader;

void attachGUIShaders()
{
	createShader(&GUIShader, "src/shaders/UI.vert",
		"src/shaders/UI.frag");
}

void initUI() {
	/*
	glGenVertexArrays(1, &vao);
	glGenBuffers(1, &vbo);
	glGenBuffers(1, &ebo);
	glBindVertexArray(vao);
	
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texPosition), texPosition, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
	
	vPosition = glGetAttribLocation(UIShader, "vPosition");
	glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 5*sizeof(GLfloat), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(vPosition);
	
	GLuint texCoord = glGetAttribLocation(UIShader, "texCoord");
	glVertexAttribPointer(texCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), BUFFER_OFFSET(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(texCoord);
	
	glBindVertexArray(0);
	*/
}


void drawUI()
{	
	/*
	glDisable(GL_DEPTH_TEST);
		
	glUseProgram(UIShader);
	
	GLuint texture1;
	int width, height;
	unsigned char* image;
	
	glGenTextures(1, &texture1);
	glBindTexture(GL_TEXTURE_2D, texture1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	image = SOIL_load_image("/Users/tylergreen/Documents/Programming/OpenGLProj/include/textures/container.jpg", 
		&width, &height, 0, SOIL_LOAD_RGB);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
	glGenerateMipmap(GL_TEXTURE_2D);
	SOIL_free_image_data(image);
	glBindTexture(GL_TEXTURE_2D, 0);
	
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture1);
	glUniform1i(glGetUniformLocation(UIShader, "newTexture1"), 0);
	
	glBindVertexArray(vao);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
		
	glEnable(GL_DEPTH_TEST);
	*/
}