#version 410

in vec4 vPosition;
in vec3 vNormal;
in vec3 vTangent;
layout(location = 4) in mat4 vInstanceMatrix;

// World-space tangent-space basis, interpolated smoothly across the surface.
// The light, view and half vectors are computed per-fragment in planet.frag
// from these plus vWorldPos; per-vertex light vectors on coarse quad-tree
// patches produced blotchy interpolated highlights.
out vec3 fT;
out vec3 fB;
out vec3 fN;
out vec3 vWorldPos;

out vec4 TexCoords;
out vec3 vLocalPos;
out float fViewDist;   // view-space distance ahead of the camera (clip.w)

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 translation;
// 1 = quad-tree LOD instance: transform the flat face-grid vertex by the
// instance matrix and project it onto the unit sphere (terrain algorithm).
// 0 = plain mesh (moons): positions are already on the unit sphere.
uniform int instanced;
// Vertex displacement. depthMap holds the height map derived from the planet
// normal map; dispAmp is the relief in fractions of planet radius (0 = off);
// dispLodBase selects the height mip for a full-face patch, and the instance
// matrix's spare slot (vInstanceMatrix[0][3]) carries the cell size.
uniform sampler2D depthMap;
uniform float dispAmp;
uniform float dispLodBase;

const float PI = 3.14159265359;
// How far below the lowest possible displaced surface point (1 - dispAmp)
// the skirt wall bottom sits; hides the worst-case crack along patch edges.
const float QT_SKIRT_MARGIN = 0.005;

// Equirectangular tangent (direction of increasing texture u), identical to
// the CPU generateTangents() that feeds the tetrahedron mesh.
vec3 sphereTangent(vec3 p)
{
	vec3 t = cross(vec3(0.0, 0.0, 1.0), p);
	if(dot(t, t) < 1e-8)
		return vec3(1.0, 0.0, 0.0); // on a pole, where the mapping degenerates
	return normalize(t);
}

void main()
{
	vec4 fPosition = vec4(vPosition.xyz, 1.0);
	bool degenerate = false;
	if(instanced == 1) {
		vec3 transformed = (fPosition * vInstanceMatrix).xyz;
		degenerate = (length(transformed) <= 0.0001);
		fPosition.xyz = degenerate ? vec3(0.0) : normalize(transformed);
	}

	// On the instanced sphere the surface normal is the (projected) position;
	// the smooth normals baked into the tetrahedron mesh equal it anyway.
	vec3 sphereN = (instanced == 1) ? fPosition.xyz : vNormal;
	vec3 sphereT = (instanced == 1) ? sphereTangent(fPosition.xyz) : vTangent;

	// Relief from the derived height map, sampled with the same equirectangular
	// mapping the fragment shader uses (so it aligns with the normal map). The
	// mip tracks the quadtree cell size: a cell half-width q on the unit face
	// spans about q radians, i.e. texelWidth*q/PI texels. Skirt vertices
	// (vPosition.w == 1, face grids only; the moon mesh never enters here)
	// skip the fetch and sit at a fixed radius below the lowest surface point,
	// so the wall hides any height mismatch along patch edges.
	if(instanced == 1 && dispAmp > 0.0 && !degenerate) {
		float isWall = vPosition.w;
		float r;
		if(isWall < 0.5) {
			vec2 huv = vec2((atan(sphereN.y, sphereN.x)/PI + 1.0) * 0.5,
			                (asin(clamp(sphereN.z, -1.0, 1.0))/PI + 0.5));
			float qs = vInstanceMatrix[0][3];
			float lod = max(0.0, log2(qs) + dispLodBase);
			float h = textureLod(depthMap, huv, lod).r;
			r = 1.0 + dispAmp * (h - 0.5) * 2.0;
		}
		else {
			r = 1.0 - 2.0*dispAmp - QT_SKIRT_MARGIN;
		}
		fPosition.xyz = sphereN * r;
	}

	gl_Position = fPosition*model*view*projection;
	fViewDist = gl_Position.w;
	// Body-local position (world minus body centre): the centre is the model
	// matrix translation column model[0][3]..model[2][3] (row-vector
	// convention; model[3][0..2] is always zero). Used for the atmospheric
	// extinction ray, whose spheres are centred on the origin.
	vLocalPos = vec3(fPosition*model) - vec3(model[0][3], model[1][3], model[2][3]);
	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vec3 T = normalize(vec3(vec4(sphereT, 0.0) * model));
	vec3 N = normalize(vec3(vec4(sphereN, 0.0)) * normalMatrix);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(N, T);

	fT = T;
	fB = B;
	fN = N;
	vWorldPos = (fPosition*model).xyz;

	TexCoords = fPosition;
}
