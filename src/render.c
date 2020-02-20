#include <fg.h>
#include <stdio.h>
#include <stdlib.h>

#include <glad/glad.h>
//#include <GL/gl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

typedef struct graph Graph;
struct graphicsContext{
	Graph g;

	float *vertices;
	unsigned int *indeces;
	
	unsigned int numVertices;
	unsigned int numIndeces;
	
	unsigned int shaderProgram, VBO, EBO, VAO;
};

struct graphicsContext gContext;

void initOpenGL(void);
void initGraph(void);
void display(void);
//void keyboard_handler(void *user_data);
//void mouse_handler(void *user_data);

#define CASE_STR( value ) case value: return #value; 
const char* eglGetErrorString( EGLint error )
{
    switch( error )
    {
    CASE_STR( EGL_SUCCESS             )
    CASE_STR( EGL_NOT_INITIALIZED     )
    CASE_STR( EGL_BAD_ACCESS          )
    CASE_STR( EGL_BAD_ALLOC           )
    CASE_STR( EGL_BAD_ATTRIBUTE       )
    CASE_STR( EGL_BAD_CONTEXT         )
    CASE_STR( EGL_BAD_CONFIG          )
    CASE_STR( EGL_BAD_CURRENT_SURFACE )
    CASE_STR( EGL_BAD_DISPLAY         )
    CASE_STR( EGL_BAD_SURFACE         )
    CASE_STR( EGL_BAD_MATCH           )
    CASE_STR( EGL_BAD_PARAMETER       )
    CASE_STR( EGL_BAD_NATIVE_PIXMAP   )
    CASE_STR( EGL_BAD_NATIVE_WINDOW   )
    CASE_STR( EGL_CONTEXT_LOST        )
    default: return "Unknown";
    }
}
#undef CASE_STR

int main(int argc, char **argv){
	static EGLint const configAttribs[] = {
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_DEPTH_SIZE, 8,
		EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
		EGL_NONE
	};

	int pbufferWidth = 256;
	int pbufferHeight = 256;

	EGLint pbufferAttribs[] = {
		EGL_WIDTH, pbufferWidth,
		EGL_HEIGHT, pbufferHeight,
		EGL_NONE,
	};

	const char *extString = (const char *)eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);

	// 1. Initialize EGL
	EGLDisplay eglDpy = eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, NULL);
	if (eglDpy == EGL_NO_DISPLAY) {
		printf("eglGetDisplay no display\n");
		return 1;
	}

	EGLint major, minor;
	if (eglInitialize(eglDpy, &major, &minor) == EGL_FALSE) {
		printf("eglInitialize failed: %s\n", eglGetErrorString(eglGetError()));
		return 1;
	}
	printf("got egl version %d %d\n", major, minor);

	// 2. Select an appropriate configuration
	EGLint numConfigs;
	EGLConfig eglCfg;

	eglChooseConfig(eglDpy, configAttribs, &eglCfg, 1, &numConfigs);

	// 3. Create a surface
	EGLSurface eglSurf = eglCreatePbufferSurface(eglDpy, eglCfg, pbufferAttribs);

	// 4. Bind the API
	eglBindAPI(EGL_OPENGL_API);

	// 5. Create a context and make it current
	EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, 
	NULL);

	eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);

	// from now on use your OpenGL context

	if (!gladLoadGL()) {
		printf("gladLoadGL failed\n");
		exit(1);
	}
	printf("OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);
		
	//load the graph into memory
	int rc = load_arg(&gContext.g, argc, argv);
	if (rc != 0) {
		printf("Could not load graph\n");
		return 1;
	}
	
	initOpenGL();
	initGraph();
	display();

	uint8_t *pixels = malloc(256 * 256 * 3 * sizeof(uint8_t));

	glReadPixels(0, 0,
	             256, 256,
	             GL_RGB,
	             GL_UNSIGNED_BYTE,
	             pixels);

	FILE *f = fopen("temp.ppm", "wb");
	fprintf(f, "P6\n");
	fprintf(f, "256 256\n");
	fprintf(f, "255\n");
	fwrite(pixels, sizeof(uint8_t), 256 * 256 * 3, f);
	fclose(f);
}

char* loadFileStr(const char *fileName){
	FILE *f;
	char *str;
	unsigned length;
	
	f = fopen(fileName, "r");
	if(!f){
		printf("Error, cannot open file %s\n", fileName);
		exit(1);
	}		

	fseek(f, 0, SEEK_END);
	length = ftell(f) + 1;
	fseek(f, 0, SEEK_SET);
	str = malloc(length*sizeof(*str));
	fread(str, sizeof(*str), length - 1, f);
	
	fclose(f);
	
	str[length-1] = '\0';
	return str;
}

void initOpenGL(){
	char *VSS;
	char *FSS;
	
	int success;
	char infoLog[512];
	
	VSS = loadFileStr("shaders/graph.vert");
	FSS = loadFileStr("shaders/graph.frag");
	
	unsigned int vertexShader = glad_glCreateShader(GL_VERTEX_SHADER);
	if (!vertexShader) { printf("ERROR: glCreateShader\n"); exit(1); }
	glShaderSource(vertexShader, 1, (const char * const*)&VSS, NULL);
	glCompileShader(vertexShader);

	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
	if(!success){
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		printf("ERROR: Vertex Shader Compilation Failed\n%s\n", infoLog);
		exit(1);
	}
	
	unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, (const char * const*)&FSS, NULL);
	glCompileShader(fragmentShader);
	
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
	if(!success){
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		printf("ERROR: Fragment Shader Compilation Failed\n%s\n", infoLog);
		exit(1);	
	}
	
	gContext.shaderProgram = glCreateProgram();
	glAttachShader(gContext.shaderProgram, vertexShader);
	glAttachShader(gContext.shaderProgram, fragmentShader);
	glLinkProgram(gContext.shaderProgram);
	
	glGetProgramiv(gContext.shaderProgram, GL_LINK_STATUS, &success);
	if(!success){
		glGetProgramInfoLog(gContext.shaderProgram, 512, NULL, infoLog);
		printf("ERROR: Shader Program Linking Failed\n%s\n", infoLog);
		exit(1);
	}
	
	glUseProgram(gContext.shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void initGraph(void){
	//fills gContext->positions and gContext->edges with correct data
	Graph * const g = &gContext.g;	
	rescale(g, -1.0, -1.0, 1.0, 1.0);
	float *positions = gContext.vertices;
	int *edges = gContext.indeces;
	
	//2 floats per position
	positions = malloc(g->ncount*2 * sizeof(*positions));
	//2 positions per edge
	edges = malloc(g->ecount*2 * sizeof(*edges));
	
	//interleave edges into contiguous buffer in-order
	for(int i = 0; i < g->ecount; ++i){
		//g->es[i]*2 because interleaved array of positions will be doubly offset
		edges[i*2 + 0] = g->es[i];
		edges[i*2 + 1] = g->et[i];

		//printf("%d\n%d\n", g->es[i], g->et[i]);
	}	
	//printf("-----------");	
	//interleave positions into contiguous buffer in-order
	for(int i = 0; i < g->ncount; ++i){
		positions[i*2 + 0] = g->nx[i];
		positions[i*2 + 1] = g->ny[i];
		
		//printf("%f\n%f\n", positions[i*2+0], positions[i*2+1]);
	}
	

	gContext.numIndeces = g->ecount*2;
	gContext.numVertices = g->ncount*2;
	
	//now openGL stuff
	//gen VBO, VAO, EBO
	glGenVertexArrays(1, &gContext.VAO);
	glGenBuffers(1, &gContext.VBO);
	glGenBuffers(1, &gContext.EBO);

	glBindVertexArray(gContext.VAO);
	
	glBindBuffer(GL_ARRAY_BUFFER, gContext.VBO);
	glBufferData(GL_ARRAY_BUFFER, g->ncount*2*sizeof(*positions), positions, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gContext.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, g->ecount*2*sizeof(*edges), edges, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(*positions), (void*)0);

	glBindAttribLocation(gContext.shaderProgram, 0, "aPos");
	
	glEnableVertexAttribArray(0);

}

void display(void){
	//renders gContext.positions

	glClear(GL_COLOR_BUFFER_BIT);	//remember GL_DEPTH_BUFFER_BIT too
	
	glBindVertexArray(gContext.VAO);
	glDrawElements(GL_LINES, gContext.numIndeces, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}
