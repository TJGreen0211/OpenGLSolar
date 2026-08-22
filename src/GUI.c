#include "GUI.h"
#include <stdio.h>

static GLuint UIShader;
static GLuint UIvao, UIvbo;
static uiButton buttons[UI_MAX_BUTTONS];
static int uiCount = 0;
static int uiReady = 0;

void attachGUIShaders()
{
	createShader(&UIShader, "src/shaders/UI.vert",
		"src/shaders/UI.frag");
}

static int uiFileExists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f)
		fclose(f);
	return f != NULL;
}

/* One shared unit quad for every button, built once. Vertex format is the
   same as before (vec3 position, vec2 tex, 5-float stride), but positions
   live in [-1,1]; the per-button rect is applied in the vertex shader. UVs
   sample v=0 at the top of the screen: loadTexture() uploads SOIL data
   without flipping it, and its first row is the top of the image file, so
   this renders icons upright. */
int uiInit(void)
{
	if (uiReady)
		return 1;

	static const float quad[] = {
		-1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
		 1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
		 1.0f,  1.0f, 0.0f,  1.0f, 0.0f,

		-1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
		 1.0f,  1.0f, 0.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,  0.0f, 0.0f
	};

	glGenVertexArrays(1, &UIvao);
	glBindVertexArray(UIvao);
	glGenBuffers(1, &UIvbo);
	glBindBuffer(GL_ARRAY_BUFFER, UIvbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);

	GLint vPosition = glGetAttribLocation(UIShader, "vPosition");
	glVertexAttribPointer(vPosition, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), BUFFER_OFFSET(0));
	glEnableVertexAttribArray(vPosition);

	GLint texCoord = glGetAttribLocation(UIShader, "texCoord");
	glVertexAttribPointer(texCoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), BUFFER_OFFSET(3 * sizeof(GLfloat)));
	glEnableVertexAttribArray(texCoord);

	glBindVertexArray(0);
	uiReady = 1;
	return 1;
}

int uiAddButton(int id, const char *icon, const char *activeIcon)
{
	if (!uiReady || uiCount >= UI_MAX_BUTTONS)
		return -1;

	uiButton *b = &buttons[uiCount];
	b->id = id;
	/* Missing art falls back to the plain pause icon so the menu still works
	   without the generated buttons/*.png. */
	const char *texPath = uiFileExists(icon) ? icon : "include/textures/buttonUp.png";
	b->texture = loadTexture(texPath);
	b->textureActive = (activeIcon && uiFileExists(activeIcon))
		? loadTexture(activeIcon) : 0;
	b->rect[0] = b->rect[1] = b->rect[2] = b->rect[3] = 0.0f;
	b->hovered = 0;
	return uiCount++;
}

int uiButtonCount(void)
{
	return uiCount;
}

/* Right-edge vertical column, top to bottom. Button size is 64 px on a
   1080p window and shrinks toward a 32 px floor as the window does. */
void uiLayoutColumn(int winW, int winH, int count)
{
	if (!uiReady || count <= 0)
		return;
	if (count > uiCount)
		count = uiCount;

	float size = (float)(winH - 2 * 20) / (float)(count * 1.25);
	if (size > 64.0f)
		size = 64.0f;
	if (size < 32.0f)
		size = 32.0f;

	float margin = size * 0.25f;
	float gap = size * 0.25f;
	float x = (float)winW - margin - size;
	float totalH = (float)count * size + (float)(count - 1) * gap;
	float y = ((float)winH - totalH) * 0.5f;
	if (y < 0.0f)
		y = 0.0f;

	for (int i = 0; i < count; i++)
	{
		buttons[i].rect[0] = x;
		buttons[i].rect[1] = y;
		buttons[i].rect[2] = size;
		buttons[i].rect[3] = size;
		y += size + gap;
	}
}

static int pointInRect(float x, float y, const float rect[4])
{
	return x >= rect[0] && x < rect[0] + rect[2] &&
	       y >= rect[1] && y < rect[1] + rect[3];
}

void uiSetMouse(float x, float y)
{
	if (!uiReady)
		return;
	for (int i = 0; i < uiCount; i++)
		buttons[i].hovered = pointInRect(x, y, buttons[i].rect);
}

int uiHitTest(float x, float y)
{
	if (!uiReady)
		return UI_ACT_NONE;
	for (int i = 0; i < uiCount; i++)
		if (pointInRect(x, y, buttons[i].rect))
			return buttons[i].id;
	return UI_ACT_NONE;
}

/* Slot + icon per button, drawn last in the frame with standard alpha
   blending and no depth test. */
void uiDrawAll(int followedId, int pauseActive)
{
	if (!uiReady || uiCount == 0)
		return;

	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	glUseProgram(UIShader);
	glUniform2f(glGetUniformLocation(UIShader, "uViewport"),
		(float)viewport[2], (float)viewport[3]);
	glUniform1i(glGetUniformLocation(UIShader, "uTex"), 0);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	glBindVertexArray(UIvao);
	glActiveTexture(GL_TEXTURE0);

	for (int i = 0; i < uiCount; i++)
	{
		uiButton *b = &buttons[i];
		int active = (b->id == UI_ACT_PAUSE)
			? pauseActive : (followedId == b->id);
		float tint = active ? 0.5f : (b->hovered ? 0.25f : 0.0f);

		/* Dark slot, slightly inset, behind the icon. */
		float inset = b->rect[2] * 0.04f;
		glUniform4f(glGetUniformLocation(UIShader, "uRect"),
			b->rect[0] + inset, b->rect[1] + inset,
			b->rect[2] - 2.0f * inset, b->rect[3] - 2.0f * inset);
		glUniform4f(glGetUniformLocation(UIShader, "uColor"), 0.04f, 0.05f, 0.09f, 0.6f);
		glUniform1f(glGetUniformLocation(UIShader, "uHasTex"), 0.0f);
		glUniform1f(glGetUniformLocation(UIShader, "uTint"), tint);
		glUniform1f(glGetUniformLocation(UIShader, "uAlpha"), 1.0f);
		glDrawArrays(GL_TRIANGLES, 0, 6);

		/* Icon on top; the active pause state swaps in its pressed texture. */
		GLuint tex = (active && b->textureActive) ? b->textureActive : b->texture;
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform4f(glGetUniformLocation(UIShader, "uRect"),
			b->rect[0], b->rect[1], b->rect[2], b->rect[3]);
		glUniform1f(glGetUniformLocation(UIShader, "uHasTex"), 1.0f);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

void uiShutdown(void)
{
	if (!uiReady)
		return;
	uiReady = 0;
	for (int i = 0; i < uiCount; i++)
	{
		if (buttons[i].texture)
			glDeleteTextures(1, &buttons[i].texture);
		if (buttons[i].textureActive)
			glDeleteTextures(1, &buttons[i].textureActive);
	}
	uiCount = 0;
	glDeleteVertexArrays(1, &UIvao);
	glDeleteBuffers(1, &UIvbo);
	glDeleteProgram(UIShader);
}
