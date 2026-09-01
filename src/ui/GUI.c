#define _USE_MATH_DEFINES
#include "GUI.h"
#include <stdio.h>
#include <string.h>

static GLuint UIShader;
static GLint uiLRect, uiLColor, uiLHasTex, uiLTint, uiLAlpha, uiLViewport, uiLTex;
static GLint uiLUVOff, uiLUVScale, uiLTintTex;
static GLuint UIvao, UIvbo;
static uiButton buttons[UI_MAX_BUTTONS];
static int uiCount = 0;
static uiSlider sliders[UI_MAX_SLIDERS];
static int sliderCount = 0;
static int activeSlider = -1;
static uiSliderChangedFn sliderChanged = NULL;
static int uiReady = 0;

static uiTextButton textButtons[UI_MAX_TEXTBUTTONS];
static int textButtonCount = 0;

// Forward: uiDrawTextButton (defined above the widget pass) reuses the flat
// quad helper that lives further down.
static void uiDrawFlat(float x, float y, float w, float h,
	float r, float g, float b, float a, float tint);

void attachGUIShaders()
{
	createShader(&UIShader, "src/shaders/UI.vert",
		"src/shaders/UI.frag");
	uiLRect = glGetUniformLocation(UIShader, "uRect");
	uiLColor = glGetUniformLocation(UIShader, "uColor");
	uiLHasTex = glGetUniformLocation(UIShader, "uHasTex");
	uiLTint = glGetUniformLocation(UIShader, "uTint");
	uiLAlpha = glGetUniformLocation(UIShader, "uAlpha");
	uiLViewport = glGetUniformLocation(UIShader, "uViewport");
	uiLTex = glGetUniformLocation(UIShader, "uTex");
	uiLUVOff = glGetUniformLocation(UIShader, "uUVOff");
	uiLUVScale = glGetUniformLocation(UIShader, "uUVScale");
	uiLTintTex = glGetUniformLocation(UIShader, "uTintTex");
}

static int uiFileExists(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (f)
		fclose(f);
	return f != NULL;
}

/* ------------------------------------------------------------------ */
/* Bitmap font                                                         */
/*                                                                     */
/* A self-contained 5x7 glyph atlas (no external font dependency). Each */
/* glyph is stored as 7 rows of 5 ASCII '0'/'1' bits and rasterised into */
/* a white-on-transparent RGBA texture at init; text draws tint it via  */
/* the uTintTex path in UI.frag. One cell per glyph, 1px padding.       */
/* ------------------------------------------------------------------ */
#define FONT_W   5
#define FONT_H   7
#define FONT_PAD 1
#define FONT_CELL_W (FONT_W + FONT_PAD * 2)
#define FONT_CELL_H (FONT_H + FONT_PAD * 2)
#define FONT_COLS 8
#define FONT_ROWS 6
#define FONT_ATLAS_W (FONT_COLS * FONT_CELL_W)
#define FONT_ATLAS_H (FONT_ROWS * FONT_CELL_H)

/* Order must match FONT_ASCII below. */
static const char *FONT_CHARS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:./+-()%,";
#define NUM_FONT_GLYPHS 46

static const char *FONT_ASCII[NUM_FONT_GLYPHS][FONT_H] = {
	/* 0  space */ { "     ", "     ", "     ", "     ", "     ", "     ", "     " },
	/* 1  '0'  */ { "01110", "10001", "10011", "10101", "11001", "10001", "01110" },
	/* 2  '1'  */ { "00100", "01100", "00100", "00100", "00100", "00100", "01110" },
	/* 3  '2'  */ { "01110", "10001", "00001", "00110", "01000", "10000", "11111" },
	/* 4  '3'  */ { "11111", "00010", "00100", "00010", "00001", "10001", "01110" },
	/* 5  '4'  */ { "00010", "00110", "01010", "10010", "11111", "00010", "00010" },
	/* 6  '5'  */ { "11111", "10000", "11110", "00001", "00001", "10001", "01110" },
	/* 7  '6'  */ { "00110", "01000", "10000", "11110", "10001", "10001", "01110" },
	/* 8  '7'  */ { "11111", "00001", "00010", "00100", "01000", "01000", "01000" },
	/* 9  '8'  */ { "01110", "10001", "10001", "01110", "10001", "10001", "01110" },
	/* 10 '9'  */ { "01110", "10001", "10001", "01111", "00001", "00010", "01100" },
	/* 11 'A'  */ { "01110", "10001", "10001", "11111", "10001", "10001", "10001" },
	/* 12 'B'  */ { "11110", "10001", "10001", "11110", "10001", "10001", "11110" },
	/* 13 'C'  */ { "01110", "10001", "10000", "10000", "10000", "10001", "01110" },
	/* 14 'D'  */ { "11100", "10010", "10001", "10001", "10001", "10010", "11100" },
	/* 15 'E'  */ { "11111", "10000", "10000", "11110", "10000", "10000", "11111" },
	/* 16 'F'  */ { "11111", "10000", "10000", "11110", "10000", "10000", "10000" },
	/* 17 'G'  */ { "01110", "10001", "10000", "10111", "10001", "10001", "01111" },
	/* 18 'H'  */ { "10001", "10001", "10001", "11111", "10001", "10001", "10001" },
	/* 19 'I'  */ { "01110", "00100", "00100", "00100", "00100", "00100", "01110" },
	/* 20 'J'  */ { "00111", "00010", "00010", "00010", "00010", "10010", "01100" },
	/* 21 'K'  */ { "10001", "10010", "10100", "11000", "10100", "10010", "10001" },
	/* 22 'L'  */ { "10000", "10000", "10000", "10000", "10000", "10000", "11111" },
	/* 23 'M'  */ { "10001", "11011", "10101", "10101", "10001", "10001", "10001" },
	/* 24 'N'  */ { "10001", "11001", "10101", "10011", "10001", "10001", "10001" },
	/* 25 'O'  */ { "01110", "10001", "10001", "10001", "10001", "10001", "01110" },
	/* 26 'P'  */ { "11110", "10001", "10001", "11110", "10000", "10000", "10000" },
	/* 27 'Q'  */ { "01110", "10001", "10001", "10001", "10101", "10010", "01101" },
	/* 28 'R'  */ { "11110", "10001", "10001", "11110", "10100", "10010", "10001" },
	/* 29 'S'  */ { "01111", "10000", "10000", "01110", "00001", "00001", "11110" },
	/* 30 'T'  */ { "11111", "00100", "00100", "00100", "00100", "00100", "00100" },
	/* 31 'U'  */ { "10001", "10001", "10001", "10001", "10001", "10001", "01110" },
	/* 32 'V'  */ { "10001", "10001", "10001", "10001", "10001", "01010", "00100" },
	/* 33 'W'  */ { "10001", "10001", "10001", "10101", "10101", "10101", "01010" },
	/* 34 'X'  */ { "10001", "10001", "01010", "00100", "01010", "10001", "10001" },
	/* 35 'Y'  */ { "10001", "10001", "01010", "00100", "00100", "00100", "00100" },
	/* 36 'Z'  */ { "11111", "00001", "00010", "00100", "01000", "10000", "11111" },
	/* 37 ':'  */ { "00000", "00100", "00000", "00000", "00000", "00100", "00000" },
	/* 38 '.'  */ { "00000", "00000", "00000", "00000", "00000", "00110", "00110" },
	/* 39 '/'  */ { "00001", "00001", "00010", "00100", "01000", "10000", "10000" },
	/* 40 '+'  */ { "00000", "00100", "00100", "11111", "00100", "00100", "00000" },
	/* 41 '-'  */ { "00000", "00000", "00000", "01110", "00000", "00000", "00000" },
	/* 42 '%'  */ { "11001", "11010", "00010", "00100", "01011", "10011", "00000" },
	/* 43 '('  */ { "00010", "00100", "01000", "01000", "01000", "00100", "00010" },
	/* 44 ')'  */ { "01000", "00100", "00010", "00010", "00010", "00100", "01000" },
	/* 45 ','  */ { "00000", "00000", "00000", "00000", "00100", "00100", "01000" },
};

static GLuint fontAtlas = 0;

static int fontIndex(char c)
{
	const char *p = strchr(FONT_CHARS, c);
	if (!p)
		return -1;
	return (int)(p - FONT_CHARS);
}

/* Rasterise every glyph into a white-on-transparent RGBA atlas. The upload
   is unflipped (first byte == lower-left), so row 0 of the font (the glyph
   top) lands at the top of its cell, matching the v mapping in uiDrawText. */
static void buildFontAtlas(void)
{
	static unsigned char buf[FONT_ATLAS_W * FONT_ATLAS_H * 4];
	memset(buf, 0, sizeof(buf));

	for (int g = 0; g < NUM_FONT_GLYPHS; g++)
	{
		int colg = g % FONT_COLS;
		int rowg = g / FONT_COLS;                 /* 0 = top row */
		int px0 = colg * FONT_CELL_W + FONT_PAD;
		int pyBase = (FONT_ROWS - 1 - rowg) * FONT_CELL_H;
		for (int tr = 0; tr < FONT_H; tr++)
			for (int cx = 0; cx < FONT_W; cx++)
			{
				if (FONT_ASCII[g][tr][cx] != '1')
					continue;
				int px = px0 + cx;
				int py = pyBase + (FONT_CELL_H - 1 - FONT_PAD) - tr;
				unsigned char *p = &buf[(py * FONT_ATLAS_W + px) * 4];
				p[0] = p[1] = p[2] = 255;
				p[3] = 255;
			}
	}

	glGenTextures(1, &fontAtlas);
	glBindTexture(GL_TEXTURE_2D, fontAtlas);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, FONT_ATLAS_W, FONT_ATLAS_H, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, buf);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);
}

/* Draw a line of bitmap-font text. Self-contained (binds the UI shader, the
   unit quad VAO and the atlas), so it can be called independently of the
   uiDrawAll button pass. x,y is the top-left of the text in window px (y
   down); px is the per-glyph-pixel scale. */
void uiDrawText(const char *text, float x, float y, float px,
                float r, float g, float b, float a)
{
	if (!uiReady || !text || fontAtlas == 0)
		return;

	int viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);

	glUseProgram(UIShader);
	glUniform2f(uiLViewport, (float)viewport[2], (float)viewport[3]);
	glUniform1i(uiLTex, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, fontAtlas);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);

	glBindVertexArray(UIvao);
	glUniform1f(uiLHasTex, 1.0f);
	glUniform1f(uiLTint, 0.0f);
	glUniform1f(uiLTintTex, 1.0f);
	glUniform1f(uiLAlpha, a);
	glUniform4f(uiLColor, r, g, b, 1.0f);
	glUniform2f(uiLUVScale, (float)FONT_W / (float)FONT_ATLAS_W,
	                        (float)FONT_H / (float)FONT_ATLAS_H);
	glUniform2f(uiLUVOff, 0.0f, 0.0f);

	float penX = x;
	for (const char *cp = text; *cp; cp++)
	{
		int gi = fontIndex(*cp);
		float advance = px * (float)(FONT_W + FONT_PAD);
		if (gi < 0)
		{
			penX += advance;
			continue;
		}
		int colg = gi % FONT_COLS;
		int rowg = gi / FONT_COLS;
		int pyBase = (FONT_ROWS - 1 - rowg) * FONT_CELL_H;
		glUniform2f(uiLUVOff,
			(colg * FONT_CELL_W + FONT_PAD) / (float)FONT_ATLAS_W,
			(pyBase + FONT_PAD) / (float)FONT_ATLAS_H);
		glUniform4f(uiLRect, penX, y, px * (float)FONT_W, px * (float)FONT_H);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		penX += advance;
	}

	glBindVertexArray(0);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glEnable(GL_CULL_FACE);
	glDisable(GL_BLEND);
}

int uiAddTextButton(int id, const char *label)
{
	if (!uiReady || textButtonCount >= UI_MAX_TEXTBUTTONS)
		return -1;
	uiTextButton *tb = &textButtons[textButtonCount];
	tb->id = id;
	if (label)
	{
		strncpy(tb->label, label, sizeof(tb->label) - 1);
		tb->label[sizeof(tb->label) - 1] = 0;
	}
	else
		tb->label[0] = 0;
	tb->rect[0] = tb->rect[1] = tb->rect[2] = tb->rect[3] = 0.0f;
	tb->selected = 0;
	tb->hovered = 0;
	return textButtonCount++;
}

int uiTextButtonCount(void)
{
	return textButtonCount;
}

void uiSetTextButtonSelected(int id, int selected)
{
	for (int i = 0; i < textButtonCount; i++)
		if (textButtons[i].id == id)
			textButtons[i].selected = selected ? 1 : 0;
}

/* Horizontal row of text buttons anchored top-left, sitting just below the
   date/speed info lines (which Main.c draws at y = 24..84). */
void uiLayoutTextButtons(int winW, int winH)
{
	(void)winW; (void)winH;
	if (!uiReady || textButtonCount <= 0)
		return;
	const float px = 1.6f;
	const float padX = 10.0f, padY = 6.0f, x0 = 24.0f, y0 = 100.0f, gap = 8.0f;
	float x = x0;
	for (int i = 0; i < textButtonCount; i++)
	{
		uiTextButton *tb = &textButtons[i];
		float tw = (float)strlen(tb->label) * px * (float)(FONT_W + FONT_PAD);
		tb->rect[0] = x;
		tb->rect[1] = y0;
		tb->rect[2] = tw + 2.0f * padX;
		tb->rect[3] = px * (float)FONT_H + 2.0f * padY;
		x += tb->rect[2] + gap;
	}
}

/* One text button: a flat slot (accent when selected, dark otherwise) with a
   centred white label. Assumes the UI shader + quad VAO + blend are already
   set up (called from uiDrawAll after the icon/slider passes). */
static void uiDrawTextButton(uiTextButton *tb)
{
	const float px = 1.6f;
	const float padX = 10.0f, padY = 6.0f;
	float x = tb->rect[0], y = tb->rect[1], w = tb->rect[2], h = tb->rect[3];

	if (tb->selected)
		uiDrawFlat(x, y, w, h, 0.30f, 0.55f, 0.95f, 0.95f, tb->hovered ? 0.25f : 0.0f);
	else
		uiDrawFlat(x, y, w, h, 0.05f, 0.06f, 0.10f, 0.70f, tb->hovered ? 0.25f : 0.0f);

	float tw = (float)strlen(tb->label) * px * (float)(FONT_W + FONT_PAD);
	float lx = x + (w - tw) * 0.5f;
	float ly = y + (h - px * (float)FONT_H) * 0.5f;
	uiDrawText(tb->label, lx, ly, px, 1.0f, 1.0f, 1.0f, 0.95f);
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
	buildFontAtlas();
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
	const char *texPath = uiFileExists(icon) ? icon : "assets/textures/buttonUp.png";
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
	for (int i = 0; i < textButtonCount; i++)
		textButtons[i].hovered = pointInRect(x, y, textButtons[i].rect) ? 1 : 0;
}

int uiHitTest(float x, float y)
{
	if (!uiReady)
		return UI_ACT_NONE;
	for (int i = 0; i < uiCount; i++)
		if (buttons[i].visible && pointInRect(x, y, buttons[i].rect))
			return buttons[i].id;
	for (int i = 0; i < textButtonCount; i++)
		if (pointInRect(x, y, textButtons[i].rect))
			return textButtons[i].id;
	return UI_ACT_NONE;
}

/* Flat (untextured) UI quad. Resets the text-only uniforms so a flat draw is
   never affected by a preceding uiDrawText (which leaves uTintTex at 1 and
   the glyph UVs set). */
static void uiDrawFlat(float x, float y, float w, float h,
	float r, float g, float b, float a, float tint)
{
	glUniform4f(uiLRect, x, y, w, h);
	glUniform4f(uiLColor, r, g, b, a);
	glUniform1f(uiLHasTex, 0.0f);
	glUniform1f(uiLTint, tint);
	glUniform1f(uiLAlpha, 1.0f);
	glUniform1f(uiLTintTex, 0.0f);
	glUniform2f(uiLUVOff, 0.0f, 0.0f);
	glUniform2f(uiLUVScale, 1.0f, 1.0f);
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
	glUniform2f(uiLViewport, (float)viewport[2], (float)viewport[3]);
	glUniform1i(uiLTex, 0);
	/* Icons and flat slots use the whole texture / no tint (overridden per
	   glyph by uiDrawText). */
	glUniform2f(uiLUVOff, 0.0f, 0.0f);
	glUniform2f(uiLUVScale, 1.0f, 1.0f);
	glUniform1f(uiLTintTex, 0.0f);

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
		glUniform4f(uiLRect,
			b->rect[0], b->rect[1], b->rect[2], b->rect[3]);
		glUniform1f(uiLHasTex, 1.0f);
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

	/* Labelled buttons last. uiDrawText is self-contained and unbinds the
	   quad on return, so re-bind it before each button's slot quad. */
	for (int i = 0; i < textButtonCount; i++)
	{
		glBindVertexArray(UIvao);
		uiDrawTextButton(&textButtons[i]);
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
	textButtonCount = 0;
	activeSlider = -1;
	glDeleteVertexArrays(1, &UIvao);
	glDeleteBuffers(1, &UIvbo);
	if (fontAtlas)
		glDeleteTextures(1, &fontAtlas);
	fontAtlas = 0;
	glDeleteProgram(UIShader);
}
