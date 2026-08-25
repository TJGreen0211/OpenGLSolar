#ifndef GUI_H
#define GUI_H
#ifdef __APPLE__
# define __gl_h_
# define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#endif

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>
#else
#include <glad/gl.h>
#endif
#include <GLFW/glfw3.h>
#include "LinearAlg.h"
#include "Common.h"

/* Action ids returned by uiHitTest / stored per button. These double as the
   camera-follow target id (followTarget), so every clickable body the camera
   can track has one: 0..10 are planet indices into planetInstanceArray,
   100+m a moon (m into moonInstanceArray), -1 the sun. The remaining
   negatives are non-target controls so a hit test is never confused with a
   "no hit" or a free camera. */
#define UI_ACT_NONE      -3   // no hit / free camera (nothing followed)
#define UI_ACT_PAUSE     -2   // pause/resume button (never a follow target)
#define UI_ACT_SUN       -1   // sun / overview follow target
#define UI_ACT_PLANET_BASE 0  // planet follow targets 0..10
#define UI_ACT_MOON_BASE   100 // moon follow targets 100+m
#define UI_ACT_TOGGLE_BASE 200 // collapsible moon-panel toggles, 200+planet

#define UI_MAX_BUTTONS 48

/* rect is in window content pixels, y-down from the top-left (the same
   space as glfwGetCursorPos). */
typedef struct uiButton {
	int id;
	GLuint texture;         // icon
	GLuint textureActive;   // alt icon for the active/pressed state (0 = reuse texture)
	float rect[4];          // x, y, w, h (from the last layout pass)
	int planet;             // owning planet index for panel buttons, -1 for the column
	int visible;            // 0 = hidden (a collapsed moon button), 1 = drawn + hit-testable
	int expanded;           // toggle buttons only: is this planet's moon row shown?
	int owned;              // 1 = this button loaded its icon (deletable in uiShutdown)
	int hovered;
} uiButton;

/* Horizontal slider. t is the handle position in [0,1]; the slider itself
   knows nothing about units — the change callback maps t to the real value.
   Pressing jumps to and grabs the handle; moving while grabbed updates t;
   releasing the mouse (anywhere) ends the grab. accent colours the fill and
   handle so each slider is distinguishable at a glance. */
typedef struct uiSlider {
	int id;
	float rect[4];   // interaction area, window px y-down
	float accent[3]; // fill + handle colour, rgb 0..1
	float t;         // 0..1
	int hovered;
	int active;
} uiSlider;

#define UI_MAX_SLIDERS 8

typedef void (*uiSliderChangedFn)(int id, float t);

void attachGUIShaders();
int  uiInit(void);
int  uiAddButton(int id, const char *icon, const char *activeIcon);
// Panel button (toggle / moon): iconTex is an already-loaded texture the
// caller owns (uiShutdown will NOT delete it), so the moon surface already
// bound in the scene can be reused as the button icon without a second load.
int  uiAddButtonTex(int id, int planet, GLuint iconTex, GLuint texActive, int visible);
int  uiButtonCount(void);
void uiLayoutColumn(int winW, int winH, int count);
// Places each planet's toggle + moon row to the left of its (already laid
// out) column button. Must run after uiLayoutColumn.
void uiLayoutPanels(int winW, int winH);
void uiSetPlanetExpanded(int planet, int expanded);
int  uiPlanetExpanded(int planet);
int  uiAddSlider(int id, float t0, const float accent[3]);
int  uiSliderCount(void);
void uiLayoutSliders(int winW, int winH, int count);
void uiSliderSetChangedFn(uiSliderChangedFn fn);
int  uiSliderPress(float x, float y); // grabs a slider under the cursor, returns its id or -1
int  uiSliderRelease(void);           // ends an active grab, returns the slider id or -1
void uiSetMouse(float x, float y);
int  uiHitTest(float x, float y);
void uiDrawAll(int followedId, int pauseActive);
void uiShutdown(void);

#define BUFFER_OFFSET( offset )   ((char*)NULL+(offset))

#endif
