#include <render.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <fg.h>
#include <zorder.h> 

#include <glad/glad.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

static void initOpenGL(struct render_ctx *ctx);
static void initGraph(struct render_ctx *ctx, int*, int);
static const char *eglGetErrorString(EGLint);

int render_main(int argc, char **argv) {
	struct render_ctx ctx;

	render_init(&ctx);
	render_display(&ctx);

	size_t size;
	void *pixels;
	size = 0;
	pixels = NULL;
	render_copy_to_buffer(&ctx, &size, &pixels);

	FILE *f = fopen("temp.ppm", "wb");
	fprintf(f, "P6\n");
	fprintf(f, "256 256\n");
	fprintf(f, "255\n");
	fwrite(pixels, sizeof(uint8_t), size, f);
	fclose(f);
}

int render_init(struct render_ctx *ctx) {
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
	EGLContext eglCtx = eglCreateContext(eglDpy, eglCfg, EGL_NO_CONTEXT, NULL);

	eglMakeCurrent(eglDpy, eglSurf, eglSurf, eglCtx);

	// from now on use your OpenGL context

	int *ZorderIndeces;
	int Zsz;

	ZorderIndeces = NULL;
	Zsz = 0;

//		ZorderIndeces = malloc((argc-1) * sizeof(*ZorderIndeces));
//		for(int i = 1; i < argc; ++i)	ZorderIndeces[i-1] = atoi(argv[i]);
//		Zsz = argc - 1;
	//todo: input checking	

	if (!gladLoadGL()) {
		printf("gladLoadGL failed\n");
		exit(1);
	}
	printf("OpenGL %d.%d\n", GLVersion.major, GLVersion.minor);
		
	//load the graph into memory
	int rc = load_arg(&ctx->g);
	if (rc != 0) {
		printf("Could not load graph\n");
		return 1;
	}
	
	initOpenGL(ctx);
	initGraph(ctx, ZorderIndeces, Zsz);
}

void render_focus_tile(struct render_ctx *ctx, int z, int x, int y) {
	float rz = 1.0;
	for (int i=0; i<z; ++i) {
		rz *= 2.0;
	}

	glUniform1f(ctx->uTranslateX, -1 * x);
	glUniform1f(ctx->uTranslateY, -1 * y);
	glUniform1f(ctx->uScale, rz);
}

void render_display(struct render_ctx *ctx) {
	glClear(GL_COLOR_BUFFER_BIT);	//remember GL_DEPTH_BUFFER_BIT too
	
	glBindVertexArray(ctx->VAO);
	glDrawElements(GL_LINES, ctx->numIndeces, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void render_copy_to_buffer(struct render_ctx *ctx, size_t *size, void **pixels) {
	size_t expsize = 256 * 256 * 3 * sizeof(uint8_t);

	if (*pixels == NULL) {
		*size = expsize;
		*pixels = malloc(*size);
	} else if (*size < expsize) {
		*size = expsize;
		*pixels = realloc(*pixels, *size);
	}

	glReadPixels(0, 0,
	             256, 256,
	             GL_RGB,
	             GL_UNSIGNED_BYTE,
	             *pixels);
}

static char* loadFileStr(const char *fileName){
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

static void initOpenGL(struct render_ctx *ctx) {
	char *VSS;
	char *FSS;
	
	int success;
	char infoLog[512];
	
	VSS = loadFileStr("shaders/graph.vert");
	FSS = loadFileStr("shaders/graph.frag");

	unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
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
	
	ctx->shaderProgram = glCreateProgram();
	glAttachShader(ctx->shaderProgram, vertexShader);
	glAttachShader(ctx->shaderProgram, fragmentShader);
	glLinkProgram(ctx->shaderProgram);
	
	glGetProgramiv(ctx->shaderProgram, GL_LINK_STATUS, &success);
	if(!success){
		glGetProgramInfoLog(ctx->shaderProgram, 512, NULL, infoLog);
		printf("ERROR: Shader Program Linking Failed\n%s\n", infoLog);
		exit(1);
	}
	
	glUseProgram(ctx->shaderProgram);

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	ctx->uTranslateX = glGetUniformLocation(ctx->shaderProgram, "uTranslateX");
	ctx->uTranslateY = glGetUniformLocation(ctx->shaderProgram, "uTranslateY");
	ctx->uScale = glGetUniformLocation(ctx->shaderProgram, "uScale");
	
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

static void initGraph(struct render_ctx *ctx, int *ZorderIndeces, int sz){
	struct graph *const g = &ctx->g;	
	float *positions = ctx->vertices;
	int *edges = ctx->indeces;

	ctx->numIndeces = 0;
	ctx->numVertices = 0;
	
	if(sz == 0){	//render whole graph normally on 1 RC
		//fills gContext->positions and gContext->edges with correct data
		rescale(0.0, 0.0, 1.0, 1.0, g);
		
		//2 floats per position
		positions = malloc(g->ncount*2 * sizeof(*positions));
		//2 positions per edge
		edges = malloc(g->ecount*2 * sizeof(*edges));
		
		//interleave edges into contiguous buffer in-order
		for(int i = 0; i < g->ecount; ++i){
			//g->es[i]*2 because interleaved array of positions will be doubly offset
			edges[i*2 + 0] = g->es[i];
			edges[i*2 + 1] = g->et[i];
		}	
		//interleave positions into contiguous buffer in-order
		for(int i = 0; i < g->ncount; ++i){
			positions[i*2 + 0] = g->nx[i];
			positions[i*2 + 1] = g->ny[i];
		}

		ctx->numIndeces = g->ecount*2;
		ctx->numVertices = g->ncount*2;

	}else{	//render only the RC specified
		
		for(int z = 0; z < sz; ++z){
			const int sz = 10;
			int curr = 0;
	
			void preinit(rc_t maxrc){ edges = malloc(maxrc * sizeof(*edges)); }
			void init(rc_t t){}
			void finish(rc_t t){}	
			void emit(rc_t rc, edgenum_t edge){
				if(rc == ZorderIndeces[z]){
					edges[curr] = edge;
					++curr;
				}
				
				printf("%d\n", rc);
			}
			partition(g, sz, preinit, init, emit, finish);
			rescale(-1.0, -1.0, 1.0, 1.0, g);
			positions = malloc(2*curr*sizeof(*edges));
	
			for(int i = 0; i < curr; ++i){
				positions[i*2 + 0] = g->nx[edges[i]];
				positions[i*2 + 1] = g->ny[edges[i]];
			}
			
			for(int i = 0; i < curr; ++i)	edges[i] = i;
			
			ctx->numIndeces += curr;
			ctx->numVertices += curr;
		}
		
		if(sz == 1){	
		//todo: change viewport to zoom into correct portion of graph if sz == 1
		}
	}	

	//now openGL stuff
	//gen VBO, VAO, EBO
	glGenVertexArrays(1, &ctx->VAO);
	glGenBuffers(1, &ctx->VBO);
	glGenBuffers(1, &ctx->EBO);

	glBindVertexArray(ctx->VAO);
	
	glBindBuffer(GL_ARRAY_BUFFER, ctx->VBO);
	glBufferData(GL_ARRAY_BUFFER, ctx->numVertices*2*sizeof(*positions), positions, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ctx->EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, ctx->numIndeces*sizeof(*edges), edges, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(*positions), (void*)0);
	
	//not needed with layout location extension enabled in shader, but put it here anyway jic
	glBindAttribLocation(ctx->shaderProgram, 0, "aPos");
	
	glEnableVertexAttribArray(0);

}

#define CASE_STR( value ) case value: return #value; 
static const char* eglGetErrorString( EGLint error )
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
