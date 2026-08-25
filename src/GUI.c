#include "GUI.h"
#include <stdio.h>

static GLuint UIShader;
static GLuint UIvao, UIvbo;
static uiButton buttons[UI_MAX_BUTTONS];
static int uiCount = 0;
static uiSlider sliders[UI_MAX_SLIDERS];
static int sliderCount = 0;
static int activeSlider = -1;
static uiSliderChangedFn sliderChanged = NULL;
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
	b->planet = -1;
	b->visible = 1;
	b->expanded = 0;
	b->owned = 1;   // loaded above; uiShutdown frees these
	b->hovered = 0;
	return uiCount++;
}

int uiAddButtonTex(int id, int planet, GLuint iconTex, GLuint texActive, int visible)
{
	if (!uiReady || uiCount >= UI_MAX_BUTTONS)
		return -1;

	uiButton *b = &buttons[uiCount];
	b->id = id;
	b->planet = planet;
	b->texture = iconTex;
	b->textureActive = texActive;   // 0 = no alternate (moon buttons)
	b->rect[0] = b->rect[1] = b->rect[2] = b->rect[3] = 0.0f;
	b->visible = visible ? 1 : 0;
	b->expanded = 0;
	b->owned = 0;   // caller owns iconTex; uiShutdown must not delete it
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

/* Moon panels hang off the right-edge column: a small chevron toggle sits
   directly to the left of a planet button, and (when that planet is expanded)
   a row of moon buttons extends further left from the toggle. Positions are
   derived from each planet's column-button rect, so this must run AFTER
   uiLayoutColumn. Sizes shrink proportionally with the column buttons. */
void uiLayoutPanels(int winW, int winH)
{
	(void)winW; (void)winH;
	if (!uiReady)
		return;

	for (int i = 0; i < uiCount; i++)
	{
		uiButton *t = &buttons[i];
		if (t->id < UI_ACT_TOGGLE_BASE)
			continue;
		int p = t->id - UI_ACT_TOGGLE_BASE;

		/* This planet's column button is the one whose action id == p
		   (0..10), unique among buttons. */
		const uiButton *col = NULL;
		for (int j = 0; j < uiCount; j++)
			if (buttons[j].id == p) { col = &buttons[j]; break; }
		if (!col)
			continue;

		float colX = col->rect[0], colY = col->rect[1], size = col->rect[2];
		float gap = size * 0.18f;
		float toggleSize = size * 0.70f;
		float moonSize = size * 0.60f;

		/* Toggle: directly left of the planet button, vertically centred. */
		float tx = colX - gap - toggleSize;
		float ty = colY + (size - toggleSize) * 0.5f;
		t->rect[0] = tx; t->rect[1] = ty;
		t->rect[2] = toggleSize; t->rect[3] = toggleSize;

		/* Moons: a row starting just left of the toggle, moon 0 closest.
		   Hidden (collapsed) buttons are still positioned so they land in the
		   right place the moment the panel re-expands. */
		float x = tx - gap - moonSize;
		float my = colY + (size - moonSize) * 0.5f;
		for (int j = 0; j < uiCount; j++)
		{
			uiButton *m = &buttons[j];
			if (m->planet != p)
				continue;
			if (m->id < UI_ACT_MOON_BASE || m->id >= UI_ACT_TOGGLE_BASE)
				continue;
			m->rect[0] = x; m->rect[1] = my;
			m->rect[2] = moonSize; m->rect[3] = moonSize;
			x -= (moonSize + gap);
		}
	}
}

/* Toggling a planet's panel shows/hides every one of its moon buttons and
   flips the toggle's expanded flag (which also swaps its chevron icon). The
   camera is left undisturbed: a followed moon keeps being followed even if
   its button hides. */
void uiSetPlanetExpanded(int planet, int expanded)
{
	if (!uiReady)
		return;
	for (int i = 0; i < uiCount; i++)
	{
		uiButton *b = &buttons[i];
		if (b->planet != planet)
			continue;
		if (b->id >= UI_ACT_TOGGLE_BASE)
			b->expanded = expanded ? 1 : 0;
		else if (b->id >= UI_ACT_MOON_BASE)
			b->visible = expanded ? 1 : 0;
	}
}

int uiPlanetExpanded(int planet)
{
	if (!uiReady)
		return 0;
	for (int i = 0; i < uiCount; i++)
		if (buttons[i].planet == planet && buttons[i].id >= UI_ACT_TOGGLE_BASE)
			return buttons[i].expanded;
	return 0;
}

static int pointInRect(float x, float y, const float rect[4])
{
	return x >= rect[0] && x < rect[0] + rect[2] &&
	       y >= rect[1] && y < rect[1] + rect[3];
}

int uiAddSlider(int id, float t0, const float accent[3])
{
	if (!uiReady || sliderCount >= UI_MAX_SLIDERS)
		return -1;

	uiSlider *s = &sliders[sliderCount];
	s->id = id;
	if (accent)
	{
		s->accent[0] = accent[0];
		s->accent[1] = accent[1];
		s->accent[2] = accent[2];
	}
	else
	{
		s->accent[0] = 0.45f;
		s->accent[1] = 0.65f;
		s->accent[2] = 1.0f;
	}
	s->t = t0 < 0.0f ? 0.0f : (t0 > 1.0f ? 1.0f : t0);
	s->rect[0] = s->rect[1] = s->rect[2] = s->rect[3] = 0.0f;
	s->hovered = 0;
	s->active = 0;
	sliderCount++;
	/* Sync the consumer with the starting position. */
	if (sliderChanged)
		sliderChanged(id, s->t);
	return sliderCount - 1;
}

int uiSliderCount(void)
{
	return sliderCount;
}

/* Stacked horizontal sliders anchored at the bottom-left corner. */
void uiLayoutSliders(int winW, int winH, int count)
{
	if (!uiReady || count <= 0)
		return;
	if (count > sliderCount)
		count = sliderCount;

	float w = (float)winW * 0.18f;
	if (w > 240.0f)
		w = 240.0f;
	if (w < 120.0f)
		w = 120.0f;
	float h = 36.0f;
	float gap = 12.0f;
	float margin = 24.0f;

	float x = margin;
	float totalH = (float)count * h + (float)(count - 1) * gap;
	float y = (float)winH - margin - totalH;
	if (y < 0.0f)
		y = 0.0f;

	for (int i = 0; i < count; i++)
	{
		sliders[i].rect[0] = x;
		sliders[i].rect[1] = y;
		sliders[i].rect[2] = w;
		sliders[i].rect[3] = h;
		y += h + gap;
	}
}

void uiSliderSetChangedFn(uiSliderChangedFn fn)
{
	sliderChanged = fn;
}

static void sliderSetT(uiSlider *s, float x)
{
	float t = (x - s->rect[0]) / s->rect[2];
	if (t < 0.0f)
		t = 0.0f;
	if (t > 1.0f)
		t = 1.0f;
	if (t != s->t)
	{
		s->t = t;
		if (sliderChanged)
			sliderChanged(s->id, t);
	}
}

int uiSliderPress(float x, float y)
{
	if (!uiReady)
		return -1;
	for (int i = 0; i < sliderCount; i++)
		if (pointInRect(x, y, sliders[i].rect))
		{
			activeSlider = i;
			sliders[i].active = 1;
			sliderSetT(&sliders[i], x);
			return sliders[i].id;
		}
	return -1;
}

int uiSliderRelease(void)
{
	if (activeSlider < 0)
		return -1;
	int id = sliders[activeSlider].id;
	sliders[activeSlider].active = 0;
	activeSlider = -1;
	return id;
}

void uiSetMouse(float x, float y)
{
	if (!uiReady)
		return;
	for (int i = 0; i < uiCount; i++)
		buttons[i].hovered = (buttons[i].visible && pointInRect(x, y, buttons[i].rect)) ? 1 : 0;
	for (int i = 0; i < sliderCount; i++)
	{
		sliders[i].hovered = pointInRect(x, y, sliders[i].rect);
		if (activeSlider == i)
			sliderSetT(&sliders[i], x);
	}
}

int uiHitTest(float x, float y)
{
	if (!uiReady)
		return UI_ACT_NONE;
	for (int i = 0; i < uiCount; i++)
		if (buttons[i].visible && pointInRect(x, y, buttons[i].rect))
			return buttons[i].id;
	return UI_ACT_NONE;
}

/* Flat (untextured) UI quad. */
static void uiDrawFlat(float x, float y, float w, float h,
	float r, float g, float b, float a, float tint)
{
	glUniform4f(glGetUniformLocation(UIShader, "uRect"), x, y, w, h);
	glUniform4f(glGetUniformLocation(UIShader, "uColor"), r, g, b, a);
	glUniform1f(glGetUniformLocation(UIShader, "uHasTex"), 0.0f);
	glUniform1f(glGetUniformLocation(UIShader, "uTint"), tint);
	glUniform1f(glGetUniformLocation(UIShader, "uAlpha"), 1.0f);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

/* Slot + icon per button and track + handle per slider, drawn last in the
   frame with standard alpha blending and no depth test. */
void uiDrawAll(int followedId, int pauseActive)
{
	if (!uiReady || (uiCount == 0 && sliderCount == 0))
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
		if (!b->visible)
			continue;   // collapsed moon buttons draw nothing

		/* Active state: a toggle glows when its planet's row is expanded (and
		   swaps to the closed chevron), the pause button when paused, and
		   every sun/planet/moon button when it is the followed body. */
		int active;
		if (b->id >= UI_ACT_TOGGLE_BASE)
			active = b->expanded;
		else if (b->id == UI_ACT_PAUSE)
			active = pauseActive;
		else
			active = (followedId == b->id);
		float tint = active ? 0.5f : (b->hovered ? 0.25f : 0.0f);

		/* Dark slot, slightly inset, behind the icon. */
		float inset = b->rect[2] * 0.04f;
		uiDrawFlat(b->rect[0] + inset, b->rect[1] + inset,
			b->rect[2] - 2.0f * inset, b->rect[3] - 2.0f * inset,
			0.04f, 0.05f, 0.09f, 0.6f, tint);

		/* Icon on top; the active pause state swaps in its pressed texture. */
		GLuint tex = (active && b->textureActive) ? b->textureActive : b->texture;
		glBindTexture(GL_TEXTURE_2D, tex);
		glUniform4f(glGetUniformLocation(UIShader, "uRect"),
			b->rect[0], b->rect[1], b->rect[2], b->rect[3]);
		glUniform1f(glGetUniformLocation(UIShader, "uHasTex"), 1.0f);
		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	for (int i = 0; i < sliderCount; i++)
	{
		uiSlider *s = &sliders[i];
		float x = s->rect[0], y = s->rect[1], w = s->rect[2], h = s->rect[3];
		float tint = s->active ? 0.5f : (s->hovered ? 0.25f : 0.0f);

		uiDrawFlat(x, y, w, h, 0.04f, 0.05f, 0.09f, 0.6f, 0.0f);

		/* Track, filled portion left of the handle, then the handle — fill
		   and handle in the slider's accent colour so each slider reads as
		   its own thing. */
		float trackX = x + 8.0f;
		float trackW = w - 16.0f;
		float trackY = y + h * 0.5f - 2.0f;
		uiDrawFlat(trackX, trackY, trackW, 4.0f, 0.22f, 0.25f, 0.32f, 0.9f, 0.0f);
		if (s->t > 0.0f)
			uiDrawFlat(trackX, trackY, trackW * s->t, 4.0f,
				s->accent[0], s->accent[1], s->accent[2], 0.9f, 0.0f);
		uiDrawFlat(trackX + trackW * s->t - 7.0f, y + h * 0.5f - 7.0f,
			14.0f, 14.0f, s->accent[0], s->accent[1], s->accent[2], 1.0f, tint);
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
		/* Only textures this button loaded are freed here; panel buttons
		   (owned == 0) reference caller-owned textures (moon surfaces, the
		   shared chevron icons) and must not be deleted. */
		if (!buttons[i].owned)
			continue;
		if (buttons[i].texture)
			glDeleteTextures(1, &buttons[i].texture);
		if (buttons[i].textureActive)
			glDeleteTextures(1, &buttons[i].textureActive);
	}
	uiCount = 0;
	sliderCount = 0;
	activeSlider = -1;
	glDeleteVertexArrays(1, &UIvao);
	glDeleteBuffers(1, &UIvbo);
	glDeleteProgram(UIShader);
}
