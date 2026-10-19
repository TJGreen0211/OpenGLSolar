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
#define COMET_TAIL_MAX     12000000.0f  // tail length at perihelion (1000x scale)
// Base speed; dTheta/frame = K / r^2, scaled per comet. Carries the 1000x
// scale twice over (r^2 in the denominator) so the visual pace is unchanged.
#define COMET_ORBIT_K      70000000000.0
#define COMET_PARTICLES    132
#define COMET_NUCLEUS_SCALE 14000.0f

typedef struct comet {
	obj mesh;
	GLuint vao, vbo;          // nucleus: positions + normals
	GLuint tailVao, quadVbo, tailVbo;
	int  particleCount;
	double theta;             // true anomaly (rad from perihelion), decreasing
	double spin;
	float q;                  // perihelion distance
	float e;                  // eccentricity = (rA - q) / (rA + q)
	float p;                  // semi-latus rectum = q (1 + e)
	float dir;                // orientation: world angle of perihelion (rad)
	double orbitK;            // per-comet speed (rad per sim-day factor: dTheta = orbitK / r^2)
	// Per-frame derived state (set by updateComet, read by Comet_Draw).
	// pos is an absolute universe position in double: subtract the camera
	// in double and downcast for the GPU (floating origin).
	dvec3 pos;
	double r;
	vec3 tailDir;
	float tailLen, tailWidth, tailAlpha;
	float vis;                  // 0..1 distance-based visibility (bright in, fades out)
} comet;

void Comet_InitGL(void);
void Comet_UpdateAll(double dDays);
void Comet_Draw(const ViewMatrices *vm);
void Comet_Shutdown(void);

#endif
