#include <fg.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <fg.h>
#include <zorder.h> 

#include <glad/glad.h>
#include <GL/freeglut.h>

typedef struct graph Graph;
typedef struct graphicsContext{
	Graph g;

	float *vertices;
	unsigned int *indeces;
	
	unsigned int numVertices;
	unsigned int numIndeces;
	
	unsigned int shaderProgram, VBO, EBO, VAO;
} GraphicsContext;
GraphicsContext gContext;

void initOpenGL(void);
void initGraph(int*, int);
void display(void);
//void keyboard_handler(void *user_data);
//void mouse_handler(void *user_data);

int main(int argc, char **argv){	
	int *ZorderIndeces;
	int Zsz;

	if(argc == 1){
		ZorderIndeces = NULL;
		Zsz = 0;
	}else{
		ZorderIndeces = malloc((argc-1) * sizeof(*ZorderIndeces));
		for(int i = 1; i < argc; ++i)	ZorderIndeces[i-1] = atoi(argv[i]);
		Zsz = argc - 1;
	}	
	//todo: input checking	
	
	if(gladLoadGL()){
		printf("No context loaded!\n");
		exit(1);
	}

	//initialize freeglut
	{	
		glutInit(&argc, argv);
		glutInitDisplayMode(GLUT_RGBA | GLUT_DEPTH);
		glutInitWindowSize(1024, 1024);
		glutInitWindowPosition(0,0);
		glutCreateWindow(argv[0]);
		glutDisplayFunc(display);
		//glutIdleFunc(display);
		//glutReshapeFunc();
		//glutKeyboardFunc(keyboard_handler);
		//glutMouseFunc(mouse_hanlder);
	}

	if(!gladLoadGL()){
		printf("Error, glad not correctly loaded\n");
		exit(1);
	}
		
	//load the graph into memory
	int rc = load_arg(&gContext.g, 1, argv);
	if (rc != 0) {
		printf("Could not load graph\n");
		return 1;
	}
	
	initOpenGL();
	initGraph(ZorderIndeces, Zsz);
	glutMainLoop();
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

void rescale2(float *positions, int sz, float x0, float y0, float x1, float y1) {
	float xmin, xmax = xmin = positions[0];
	float ymin, ymax = ymin = positions[1];
	for(size_t i = 2; i < sz; i += 2){
		if (positions[i] < xmin) xmin = positions[i]; 
		if (positions[i] > xmax) xmax = positions[i]; 
	}
	for(size_t i = 3; i < sz; i += 2){
		if(positions[i] < ymin) ymin = positions[i];
		if(positions[i] > ymax) ymax = positions[i];
	}	
	for(size_t i = 0; i < sz; i += 2) 	positions[i] = (positions[i] - xmin) / (xmax - xmin) * (x1 - x0) + x0;
	for(size_t i = 1; i < sz; i += 2) 	positions[i] = (positions[i] - ymin) / (ymax - ymin) * (y1 - y0) + y0;
}

void initGraph(int *ZorderIndeces, int sz){
	
	Graph * const g = &gContext.g;	
	float *positions = gContext.vertices;
	int *edges = gContext.indeces;

	gContext.numIndeces = 0;
	gContext.numVertices = 0;
	
	if(sz == 0){	//render whole graph normally on 1 RC
		//fills gContext->positions and gContext->edges with correct data
		rescale(g, -1.0, -1.0, 1.0, 1.0);
		
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

		gContext.numIndeces = g->ecount*2;
		gContext.numVertices = g->ncount*2;

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
			rescale(g, -1.0, -1.0, 1.0, 1.0);	
			positions = malloc(2*curr*sizeof(*edges));
	
			for(int i = 0; i < curr; ++i){
				positions[i*2 + 0] = g->nx[edges[i]];
				positions[i*2 + 1] = g->ny[edges[i]];
			}
			
			for(int i = 0; i < curr; ++i)	edges[i] = i;
			
			gContext.numIndeces += curr;
			gContext.numVertices += curr;
		}
		
		if(sz == 1){	
		//todo: change viewport to zoom into correct portion of graph if sz == 1
		}
	}	

	//now openGL stuff
	//gen VBO, VAO, EBO
	glGenVertexArrays(1, &gContext.VAO);
	glGenBuffers(1, &gContext.VBO);
	glGenBuffers(1, &gContext.EBO);

	glBindVertexArray(gContext.VAO);
	
	glBindBuffer(GL_ARRAY_BUFFER, gContext.VBO);
	glBufferData(GL_ARRAY_BUFFER, gContext.numVertices*2*sizeof(*positions), positions, GL_STATIC_DRAW);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gContext.EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, gContext.numIndeces*sizeof(*edges), edges, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(*positions), (void*)0);
	
	//not needed with layout location extension enabled in shader, but put it here anyway jic
	glBindAttribLocation(gContext.shaderProgram, 0, "aPos");
	
	glEnableVertexAttribArray(0);

}

void display(void){
	glClear(GL_COLOR_BUFFER_BIT);	//remember GL_DEPTH_BUFFER_BIT too
	
	glBindVertexArray(gContext.VAO);
	glDrawElements(GL_LINES, gContext.numIndeces, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
	
	glutSwapBuffers();
	glutPostRedisplay();
}
