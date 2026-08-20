#include "Camera.h"
#include <math.h>

const float toRadians = M_PI / 180.0f;

#define ARCBALL_RADIUS 1.0f

/* Orbit radius: how far the camera sits from the pivot the scene rotates
   around. The old camera orbited at distance 1 (inside the sun); 5000
   frames the inner planets at the default 45 deg FOV. Scroll dollies this
   between 0 (camera at the pivot) and the fixed maximum below. */
extern float CameraDistance = 5000.0f;
extern const float CameraDistanceMax = 25000.0f;
extern GLfloat MovementSpeed = 5000.0f;

/* Quaternion per the arcball article: cosine is the cosine of half the
   rotation angle, axis is the unit rotation axis scaled by the sine of
   half the angle, so q = [cosine, axis] = cos(theta/2) + axis*sin(theta/2). */
typedef struct Quaternion
{
    float cosine;
    vec3 axis;
} Quaternion;

typedef struct Arcball
{
    vec3 startPos;
    vec3 currentPos;
    vec3 startPosUnitVector;
    vec3 currentPosUnitVector;
    Quaternion currentQuaternion;
    Quaternion lastQuaternion;
    float cosValue;
    float cosValue_2;
    float theta;
    float angle;
    vec3 rotationalAxis;
    vec3 rotationalAxis_2;
} Arcball;

/* lastQuaternion starts at the identity quaternion; dragging composes
   onto it relatively. (The article seeds it with [0, (1,0,0)], a 180 deg
   x-flip that only made sense for its default scene orientation.) */
static Arcball arcball = {
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.0f},
	{0.0f, {1.0f, 0.0f, 0.0f}},
	{1.0f, {0.0f, 0.0f, 0.0f}},
	0.0f,
	1.0f,
	0.0f,
	0.0f,
	{1.0f, 0.0f, 0.0f},
	{0.0f, 0.0f, 0.0f}
};

static vec3 Pivot = {0.0f, 0.0f, 0.0f};
static int dragging = 0;

vec3 Front = {0.0f, 0.0f, -1.0f};
vec3 Up = {0.0f, 1.0f, 0.0f};
vec3 Right = {1.0f, 0.0f, 0.0f};

static float zAxis(float x, float y)
{
	float length = sqrtf(x * x + y * y);
	if (length <= ARCBALL_RADIUS)
		return (float)sqrtf(ARCBALL_RADIUS * ARCBALL_RADIUS - x * x - y * y);
	return 0.0f;
}

static void projectCursorToSphere(GLFWwindow *window, double xPos, double yPos, vec3 *out)
{
	int windowWidth, windowHeight;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);
	out->x = (((float)xPos - (windowWidth / 2.0f)) / (windowWidth / 2.0f)) * ARCBALL_RADIUS;
	out->y = (((windowHeight / 2.0f) - (float)yPos) / (windowHeight / 2.0f)) * ARCBALL_RADIUS;
	out->z = zAxis(out->x, out->y);
}

static void arcballRotation(void)
{
	arcball.startPosUnitVector = normalizevec3(arcball.startPos);
	arcball.currentPosUnitVector = normalizevec3(arcball.currentPos);
	arcball.currentQuaternion.axis = crossvec3(arcball.startPos, arcball.currentPos);
	arcball.currentQuaternion.axis = normalizevec3(arcball.currentQuaternion.axis);

	arcball.cosValue = dotvec3(arcball.startPosUnitVector, arcball.currentPosUnitVector);
	if (arcball.cosValue > 1.0f)
		arcball.cosValue = 1.0f;
	if (arcball.cosValue < -1.0f)
		arcball.cosValue = -1.0f;
	arcball.theta = acosf(arcball.cosValue) * 180.0f / (float)M_PI;

	float halfAngle = (arcball.theta / 2.0f) * toRadians;
	arcball.currentQuaternion.cosine = cosf(halfAngle);
	arcball.currentQuaternion.axis = scalarMultvec3(arcball.currentQuaternion.axis, sinf(halfAngle));

	/* q' = q_current * q_last */
	arcball.cosValue_2 = (arcball.currentQuaternion.cosine * arcball.lastQuaternion.cosine)
		- dotvec3(arcball.currentQuaternion.axis, arcball.lastQuaternion.axis);
	if (arcball.cosValue_2 > 1.0f)
		arcball.cosValue_2 = 1.0f;
	if (arcball.cosValue_2 < -1.0f)
		arcball.cosValue_2 = -1.0f;

	vec3 temporaryVector;
	temporaryVector = crossvec3(arcball.currentQuaternion.axis, arcball.lastQuaternion.axis);

	arcball.rotationalAxis_2.x = (arcball.currentQuaternion.cosine * arcball.lastQuaternion.axis.x)
		+ (arcball.lastQuaternion.cosine * arcball.currentQuaternion.axis.x) + temporaryVector.x;
	arcball.rotationalAxis_2.y = (arcball.currentQuaternion.cosine * arcball.lastQuaternion.axis.y)
		+ (arcball.lastQuaternion.cosine * arcball.currentQuaternion.axis.y) + temporaryVector.y;
	arcball.rotationalAxis_2.z = (arcball.currentQuaternion.cosine * arcball.lastQuaternion.axis.z)
		+ (arcball.lastQuaternion.cosine * arcball.currentQuaternion.axis.z) + temporaryVector.z;

	arcball.angle = (acosf(arcball.cosValue_2) * 180.0f / (float)M_PI) * 2.0f;

	float halfScale = sinf((arcball.angle / 2.0f) * toRadians);
	if (fabsf(halfScale) > 0.000001f)
		arcball.rotationalAxis = dividevec3(arcball.rotationalAxis_2, halfScale);
	else
	{
		arcball.rotationalAxis.x = 1.0f;
		arcball.rotationalAxis.y = 0.0f;
		arcball.rotationalAxis.z = 0.0f;
	}
}

mat4 getViewRotation()
{
	mat4 r = IDENTITY_MATRIX;
	float angle = arcball.angle * toRadians;
	float c = cosf(angle);
	float s = sinf(angle);
	float t = 1.0f - c;
	vec3 axis = normalizevec3(arcball.rotationalAxis);

	/* Rodrigues rotation matrix, same layout as rotateX/rotateY. */
	r.m[0][0] = c + t * axis.x * axis.x;
	r.m[0][1] = t * axis.x * axis.y - s * axis.z;
	r.m[0][2] = t * axis.x * axis.z + s * axis.y;
	r.m[1][0] = t * axis.x * axis.y + s * axis.z;
	r.m[1][1] = c + t * axis.y * axis.y;
	r.m[1][2] = t * axis.y * axis.z - s * axis.x;
	r.m[2][0] = t * axis.x * axis.z - s * axis.y;
	r.m[2][1] = t * axis.y * axis.z + s * axis.x;
	r.m[2][2] = c + t * axis.z * axis.z;
	return r;
}

mat4 getViewPosition()
{
	mat4 r = getViewRotation();
	/* t is the translation that puts the pivot d units straight ahead on
	   the view axis: R * (pivot + t) == (0, 0, -d). */
	float tx = -CameraDistance * r.m[2][0] - Pivot.x;
	float ty = -CameraDistance * r.m[2][1] - Pivot.y;
	float tz = -CameraDistance * r.m[2][2] - Pivot.z;
	return translate(tx, ty, tz);
}

mat4 getViewMatrix()
{
	return multiplymat4(getViewRotation(), getViewPosition());
}

vec3 getCameraPosition()
{
	mat4 r = getViewRotation();
	vec3 c;
	c.x = CameraDistance * r.m[2][0] + Pivot.x;
	c.y = CameraDistance * r.m[2][1] + Pivot.y;
	c.z = CameraDistance * r.m[2][2] + Pivot.z;
	return c;
}

/* The view transform is v_cam = R * (v_world + t), so a world direction d
   appears in the camera as R * d. The world direction that shows up as the
   camera's screen +x / +y / -z is therefore R^T * e_axis -- a ROW of R, not
   a column. Rows and columns only coincide for identity-like rotations. */
void updateCameraVectors()
{
	mat4 r = getViewRotation();
	vec3 right = {r.m[0][0], r.m[0][1], r.m[0][2]};
	vec3 front = {-r.m[2][0], -r.m[2][1], -r.m[2][2]};
	vec3 up = {r.m[1][0], r.m[1][1], r.m[1][2]};
	Right = right;
	Front = front;
	Up = up;
}

void processKeyboard(enum Camera_Movement direction, GLfloat deltaTime)
{
	GLfloat velocity = MovementSpeed * deltaTime;
	updateCameraVectors();

    if (direction == FORWARD)
        Pivot = plusequalvec3(Pivot, scalarMultvec3(Front, velocity));
    if (direction == BACKWARD)
        Pivot = minusequalvec3(Pivot, scalarMultvec3(Front, velocity));
    if (direction == LEFT)
        Pivot = minusequalvec3(Pivot, scalarMultvec3(Right, velocity));
    if (direction == RIGHT)
        Pivot = plusequalvec3(Pivot, scalarMultvec3(Right, velocity));
}

void processMousePress(GLFWwindow *window)
{
	double startXPos, startYPos;
	glfwGetCursorPos(window, &startXPos, &startYPos);
	projectCursorToSphere(window, startXPos, startYPos, &arcball.startPos);
	dragging = 1;
}

void processMouseRelease(void)
{
	if (dragging)
	{
		arcball.lastQuaternion.cosine = arcball.cosValue_2;
		arcball.lastQuaternion.axis = arcball.rotationalAxis_2;
	}
	dragging = 0;
}

void processMouseMovement(GLFWwindow *window, GLfloat xpos, GLfloat ypos)
{
	if (!dragging)
		return;
	projectCursorToSphere(window, (double)xpos, (double)ypos, &arcball.currentPos);
	arcballRotation();
}

/* Scroll in (yoffset < 0) shrinks the orbit radius to 0, scroll out
   (yoffset > 0) grows it to the fixed maximum. */
void processMouseScroll(GLfloat yoffset)
{
	CameraDistance += yoffset * (CameraDistanceMax * 0.06f);
	if (CameraDistance < 0.0f)
		CameraDistance = 0.0f;
	if (CameraDistance > CameraDistanceMax)
		CameraDistance = CameraDistanceMax;
}
