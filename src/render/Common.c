#include "Common.h"
#include "LoadShader.h"
#include <stdio.h>

//GLfloat deltaTime = 0.0f;
//GLfloat lastFrame = 0.0f;

void createShader(GLuint *shader, char *vert, char *frag)
{
	GLuint vertShader = LoadShader(vert, GL_VERTEX_SHADER);
    GLuint fragShader = LoadShader(frag, GL_FRAGMENT_SHADER);
    *shader = glCreateProgram();
    glAttachShader(*shader, vertShader);
    glAttachShader(*shader, fragShader);
    glLinkProgram(*shader);

    GLint success;
    glGetProgramiv(*shader, GL_LINK_STATUS, &success);
    if(success == GL_FALSE)
    {
        char log[1024];
        glGetProgramInfoLog(*shader, sizeof(log), 0, log);
        fprintf(stderr, "shader link info: \n %s \n", log);
    }
}

GLuint loadCubemap(char **faces)
{
	GLuint textureID;
    glGenTextures(1, &textureID);
    glActiveTexture(GL_TEXTURE0);

    int width, height;
    unsigned char* image;

    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    for (GLuint i = 0; i < 6; i++)
    {
        image = SOIL_load_image(faces[i], &width, &height, 0, SOIL_LOAD_RGB);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    	
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return textureID;
}


GLuint loadTexture(char const * path)
{
    //Generate texture ID and load texture data
    GLuint textureID;
    glGenTextures(1, &textureID);
    int width = 0, height = 0;
    unsigned char* image = SOIL_load_image(path, &width, &height, 0, SOIL_LOAD_RGB);
    // Assign texture to ID
    glBindTexture(GL_TEXTURE_2D, textureID);
    if (image == NULL || width <= 0 || height <= 0)
    {
        // No file: upload a valid neutral texture (flat height) instead of
        // calling glTexImage2D with garbage size/pointer.
        static const unsigned char neutral[3] = { 0, 0, 0 };
        fprintf(stderr, "[trace] loadTexture: FAILED '%s', using neutral 1x1 fallback\n", path);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, neutral);
    }
    else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    SOIL_free_image_data(image);
    return textureID;
}

/*
GLfloat frameCount()
{
	GLfloat currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    return lastFrame;
}*/

static int   mvpLastProg = -1, mvpLastFrame = -1;
static GLint mvpLProj, mvpLModel, mvpLView;

/* proj/view are identical for every call in a frame; only model differs. Keying
   the cache on (program, frame) uploads proj/view once per program per frame.
   Never drop the frame key: a mid-frame program switch must
   refetch, or the cached locations would belong to another program. */
void glSetupMVP(GLuint shader, mat4 model, const ViewMatrices *vm, int frame)
{
	if (shader != mvpLastProg || frame != mvpLastFrame)
	{
		mvpLProj   = glGetUniformLocation( shader, "projection" );
		mvpLView   = glGetUniformLocation( shader, "view" );
		mvpLModel  = glGetUniformLocation( shader, "model" );
		glUniformMatrix4fv(mvpLProj, 1, GL_FALSE, &vm->proj.m[0][0]);
		glUniformMatrix4fv(mvpLView, 1, GL_FALSE, &vm->view.m[0][0]);
		mvpLastProg  = shader;
		mvpLastFrame = frame;
	}
	glUniformMatrix4fv( mvpLModel, 1, GL_FALSE, &model.m[0][0] );
}

void bindTexture(GLuint activeTex, GLuint tex) {
    glActiveTexture(activeTex);
    glBindTexture(GL_TEXTURE_2D, tex);
}

void glSetupPlanetLighting(GLuint program)
{
    vec4 light_ambient = {0.2f, 0.2f, 0.2f, 1.0f};
    vec4 light_diffuse = {1.0f, 1.0f, 1.0f, 1.0f};
    vec4 light_specular = {1.0f, 1.0f, 1.0f, 1.0f};

    vec4 material_ambient = {0.2f, 0.2f, 1.0f, 1.0f};
    vec4 material_diffuse = {0.8f, 0.8f, 0.8f, 1.0f};
    vec4 material_specular = {0.5f, 0.5f, 0.5f, 1.0f};

    vec4 light_position = {0.0f, 0.0f, 1.0f, 1.0f};
    float material_shininess = 50.0f;

    vec4 ambient_product = multiplyvec4(light_ambient, material_ambient);
    vec4 diffuse_product = multiplyvec4(light_diffuse, material_diffuse);
    vec4 specular_product = multiplyvec4(light_specular, material_specular);

    glUniform4fv( glGetUniformLocation(program, "ambientProduct"), 1, (float*)(&ambient_product) );
    glUniform4fv( glGetUniformLocation(program, "diffuseProduct"), 1, (float*)(&diffuse_product) );
    glUniform4fv( glGetUniformLocation(program, "specularProduct"), 1, (float*)(&specular_product) );
	glUniform4fv( glGetUniformLocation(program, "lightPos"), 1, (float*)(&light_position) );
	glUniform1f ( glGetUniformLocation(program, "shininess"), material_shininess );
}

// Deterministic shared LCG (same constants as the old beltRand). Belts and
// comets reseed + draw from this one instance in init-belts order, keeping the
// randomized-but-reproducible layout bit-identical to the old stream.
static unsigned int sceneSeed = 0u;
float SceneRand(void)
{
	sceneSeed = sceneSeed * 1664525u + 1013904223u;
	return (float)(sceneSeed >> 8) / (float)(1u << 24);
}
void SceneReseed(unsigned int seed)
{
	sceneSeed = seed;
}