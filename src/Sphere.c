#include "Sphere.h"
int NumTimesToSubdivide;
int NumTriangles;
int NumVertices;

sphere newSphere;

int Index = 0;
void triangle(vec3 a, vec3 b, vec3 c)
{
	vec3 one, two;
	one.x = b.x - a.x;
	one.y = b.y - a.y;
	one.z = b.z - a.z;
	
	two.x = c.x - b.x;
	two.y = c.y - b.y;
	two.z = c.z - b.z;

	vec3 normal = normalizevec3(crossvec3(one, two));
	newSphere.normals[Index] = normal; newSphere.points[Index] = a; Index++;
	newSphere.normals[Index] = normal; newSphere.points[Index] = b; Index++;
	newSphere.normals[Index] = normal; newSphere.points[Index] = c; Index++;
}

vec3 unitCircle(vec3 p)
{
	float length = p.x*p.x + p.y*p.y + p.z*p.z;
	vec3 t = {0.0, 0.0, 0.0};
	if(length > 0)
	{
		t.x = p.x / sqrtf(length);
		t.y = p.y / sqrtf(length);
		t.z = p.z / sqrtf(length);
	}
	return t;
}

void divideTriangle(vec3 a, vec3 b, vec3 c, int count)
{
	if(count > 0) {
		vec3 v1 = unitCircle(addvec3(a, b));
		vec3 v2 = unitCircle(addvec3(a, c));
		vec3 v3 = unitCircle(addvec3(b, c));
		divideTriangle(  a, v1, v2, count - 1 );
        divideTriangle(  c, v2, v3, count - 1 );
        divideTriangle(  b, v3, v1, count - 1 );
        divideTriangle( v1, v3, v2, count - 1 );
	}
	else {
		triangle(a, b, c);
	}
}

sphere tetrahedron(int count)
{
	NumTimesToSubdivide = count;
	NumTriangles = (int)pow(4, count+1);
	NumVertices = 3 * NumTriangles;
	
	int mallocVertSize = NumVertices*sizeof(vec3);
	int mallocNormSize = NumVertices*sizeof(vec3);
	
	newSphere.points = malloc(mallocVertSize);
	newSphere.normals = malloc(mallocNormSize);
	
	vec3 v[4] = {
		{ 0.0f, 0.0f, 1.0f },
		{ 0.0f, 0.942809f, -0.333333f},
		{ -0.816497f, -0.471405f, -0.333333f },
		{ 0.816497f, -0.471405f, -0.333333f }
    };
    
    divideTriangle( v[0], v[1], v[2], count );
    divideTriangle( v[3], v[2], v[1], count );
    divideTriangle( v[0], v[3], v[1], count );
    divideTriangle( v[0], v[2], v[3], count );
    
    newSphere.vertexNumber = NumVertices;
    newSphere.size = mallocVertSize;
    newSphere.nsize = mallocNormSize;
	return newSphere;
}