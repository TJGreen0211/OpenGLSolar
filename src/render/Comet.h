#ifndef COMET_H
#define COMET_H

#include "Common.h"

// Highly-eccentric-ellipse comet with a billboarded particle tail. The nucleus
// is a small rock; the tail is a fan of camera-facing soft sprites run
// anti-sunward from the nucleus, with length and brightness both proportional
// to 1/r^2.
// Number of comets in the scene. Each gets its own perihelion distance,
// aphelion distance, approach angle, orbit phase, and speed (seeded in
// initComets), so they cross the system at different intervals from
// different directions.
#define COMET_COUNT        3
// Each comet is a flat highly-eccentric ellipse (sun at the focus): a close
// perihelion q (dive into the sun) and a far aphelion (long arc out into the
// system). Its visibility is tied to distance from the sun exactly like a
// real comet: fully bright close in, smoothly fading to invisible beyond
// COMET_VIS_FAR * q. The full anomaly wrap at +/-PI happens at aphelion while
// faded out, so the slow far arc is never seen and what the eye catches is
// the fast, bright close pass. Keeping this tight (~2.6q) means there is no
// "faint dot drifting in the distance" at all.
#define COMET_VIS_FAR      2.6f      // invisible beyond this many perihelion distances
#define COMET_TAIL_MAX     12000.0f  // tail length at perihelion
#define COMET_ORBIT_K      70000.0f  // base speed; dTheta/frame = K / r^2, scaled per comet
#define COMET_PARTICLES    132
#define COMET_NUCLEUS_SCALE 14.0f

typedef struct comet {
	obj mesh;
	GLuint vao, vbo;          // nucleus: positions + normals
	GLuint tailVao, quadVbo, tailVbo;
	int  particleCount;
	float theta;              // true anomaly (rad from perihelion), decreasing
	float q;                  // perihelion distance
	float e;                  // eccentricity = (rA - q) / (rA + q)
	float p;                  // semi-latus rectum = q (1 + e)
	float dir;                // orientation: world angle of perihelion (rad)
	float orbitK;             // per-comet speed (dTheta per 60-Hz reference frame = orbitK / r^2)
	float spin;
	// Per-frame derived state (set by updateComet, read by Comet_Draw).
	vec3 pos;
	float r;
	vec3 tailDir;
	float tailLen, tailWidth, tailAlpha;
	float vis;                  // 0..1 distance-based visibility (bright in, fades out)
} comet;

void Comet_InitGL(void);
void Comet_UpdateAll(float dDays);
void Comet_Draw(const ViewMatrices *vm);
void Comet_Shutdown(void);

#endif
