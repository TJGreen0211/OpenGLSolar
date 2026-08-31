#include "Obj.h"

obj ObjLoadModel(char *fname);
obj newObj;

// First integer component of an OBJ face element. Handles "3", "3/7",
// "3//7", "3/7/1" and the negative (relative) forms of each.
static int faceIndex(char *token)
{
	int sign = 1;
	int i = 0;
	if(*token == '-')
	{
		sign = -1;
		token++;
	}
	while(*token >= '0' && *token <= '9')
		i = i * 10 + (*token++ - '0');
	return sign * i;
}

obj ObjLoadModel(char *fname)
{
	FILE *fp = fopen(fname, "r");
	if(!fp)
	{
		printf("Error while opening the file.\n");
		exit(1);
	}

	// Pass 1: count vertices and faces. Line-based scanning keeps this
	// independent of whitespace parity (the old fgetc loop skipped chars).
	char line[512];
	int vertCount = 0;
	int faceCount = 0;
	while(fgets(line, sizeof(line), fp))
	{
		if(line[0] == 'v' && (line[1] == ' ' || line[1] == '\t'))
			vertCount++;
		else if(line[0] == 'f' && (line[1] == ' ' || line[1] == '\t'))
			faceCount++;
	}
	rewind(fp);

	vec3 *verts = malloc(sizeof(vec3) * vertCount);
	vec3 *faces = malloc(sizeof(vec3) * faceCount);
	int i = 0;
	int j = 0;

	// Pass 2: fill them. Only `v` and `f` matter; `vt`/`vn`/`o`/`mtllib`/
	// comments are skipped by the prefix checks, and `f v/vt/vn` faces are
	// reduced to their position indices so Blender exports load as-is.
	while(fgets(line, sizeof(line), fp))
	{
		if(line[0] == 'v' && (line[1] == ' ' || line[1] == '\t'))
		{
			float x, y, z;
			sscanf(line, "v %f %f %f", &x, &y, &z);
			verts[i].x = x;
			verts[i].y = y;
			verts[i].z = z;
			i++;
		}
		else if(line[0] == 'f' && (line[1] == ' ' || line[1] == '\t'))
		{
			char *toks[3];
			int nt = 0;
			for(char *tok = strtok(line + 2, " \t\r\n"); tok && nt < 3; tok = strtok(NULL, " \t\r\n"))
				toks[nt++] = tok;
			if(nt >= 3)
			{
				faces[j].x = (float)faceIndex(toks[0]);
				faces[j].y = (float)faceIndex(toks[1]);
				faces[j].z = (float)faceIndex(toks[2]);
				j++;
			}
		}
	}

	fclose(fp);

	// Mesh centroid, used to tell outward from inward face normals.
	vec3 centroid = { 0.0f, 0.0f, 0.0f };
	for(int k = 0; k < vertCount; k++)
	{
		centroid.x += verts[k].x / (float)vertCount;
		centroid.y += verts[k].y / (float)vertCount;
		centroid.z += verts[k].z / (float)vertCount;
	}

	newObj.normals = malloc(faceCount*3*sizeof(vec3));
	newObj.points = malloc(faceCount*3*sizeof(vec3));

	int Index = 0;
	for(int fi = 0; fi < faceCount; fi++)
	{
		// Negative indices are relative to the end of the vertex list.
		int a = (int)faces[fi].x - 1;
		int b = (int)faces[fi].y - 1;
		int c = (int)faces[fi].z - 1;
		if(a < 0) a += vertCount;
		if(b < 0) b += vertCount;
		if(c < 0) c += vertCount;

		vec3 one, two;
		one.x = verts[b].x - verts[a].x;
		one.y = verts[b].y - verts[a].y;
		one.z = verts[b].z - verts[a].z;

		two.x = verts[c].x - verts[b].x;
		two.y = verts[c].y - verts[b].y;
		two.z = verts[c].z - verts[b].z;

		vec3 normal = normalizevec3(crossvec3(one, two));

		// Enforce consistent outward winding (rocks/planets are convex and
		// contain their centroid). Some exporters write CW faces, which would
		// cull the visible side under the app's global GL_BACK culling and
		// light the wrong face.
		vec3 fc;
		fc.x = (verts[a].x + verts[b].x + verts[c].x) / 3.0f - centroid.x;
		fc.y = (verts[a].y + verts[b].y + verts[c].y) / 3.0f - centroid.y;
		fc.z = (verts[a].z + verts[b].z + verts[c].z) / 3.0f - centroid.z;
		if(normal.x * fc.x + normal.y * fc.y + normal.z * fc.z < 0.0f)
		{
			int t;
			t = b; b = c; c = t;
			normal.x = -normal.x;
			normal.y = -normal.y;
			normal.z = -normal.z;
		}

		newObj.normals[Index] = normal; newObj.points[Index] = verts[a]; Index++;
		newObj.normals[Index] = normal; newObj.points[Index] = verts[b]; Index++;
		newObj.normals[Index] = normal; newObj.points[Index] = verts[c]; Index++;
	}

	free(verts);
	free(faces);

	newObj.size = Index*sizeof(vec3);
	newObj.nsize = Index*sizeof(vec3);
	newObj.vertexNumber = Index;
	return newObj;
}
