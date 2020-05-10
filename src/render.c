#include "render.h"
#include <stdio.h>
#include <stdlib.h>

#include "MAB/log.h"

#include <glad/glad.h>
#define GLAPIENTRY APIENTRY
#include <GL/osmesa.h>

struct attrib {
	GLuint size;
	GLenum type;
	GLuint count;
	union {
		GLbyte range[8];
		GLfloat frange[2];
		GLint irange[2];
		GLuint urange[2];
	};
	union {
		GLbyte *data;
		GLfloat *floats;
		GLint *ints;
		GLuint *uints;
	};
};

void *render(void *v) {
	OSMesaContext context;
	GLsizei resolution, nedges;
	GLubyte *image;
	GLuint i, ncount, vertexShader, fragmentShader, program, indexBuffer, aNodeBuffers[16];
	GLint rc, uTranslateX, uTranslateY, uScale, aNodeLocations[16];
	struct graph *graph;
	GLchar log[512];
	const GLchar *const vertexShaderSource, *const fragmentShaderSource;
	struct attrib *nattribs;
	struct attrib *edges;
	struct render_params *params;
	FILE *f;
	int where;

	params = v;

	for (;;)
	switch (where) {
	case WAIT:
	
	break;

	case INIT_OSMESA:
	MAB_WRAP("create osmesa context") {
		context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
		if(!context) {
			fprintf(stderr, "could not init OSMesa context\n");
			exit(1);
		}

		image = malloc(4 * resolution * resolution);
		if (!image) {
			fprintf(stderr, "could not allocate image buffer\n");
			exit(1);
		}

		rc = OSMesaMakeCurrent(context, image, GL_UNSIGNED_BYTE, resolution, resolution);
		if(!rc) {
			fprintf(stderr, "could not bind to image buffer\n");
			exit(1);
		}
	}
	__attribute__((fallthrough));

	case INIT_OPENGL:
	MAB_WRAP("load opengl") {
		if (!gladLoadGLLoader((GLADloadproc)OSMesaGetProcAddress)) {
			printf("gladLoadGL failed\n");
			exit(1);
		}
		printf("OpenGL %s\n", glGetString(GL_VERSION));
	}
	__attribute__((fallthrough));

	case INIT_PROGRAM:
	MAB_WRAP("create program") {
		MAB_WRAP("create vertex shader") {
			if (vertexShader) glDeleteShader(vertexShader);
			vertexShader = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
			glCompileShader(vertexShader);

			glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &rc);
			if (!rc) {
				glGetShaderInfoLog(vertexShader, sizeof(log), NULL, log);
				printf("ERROR: Vertex Shader Compilation Failed\n%s\n", log);
				return 1;
			}
		}

		MAB_WRAP("create fragment shader") {
			if (fragmentShader) glDeleteShader(fragmentShader);
			fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
			glCompileShader(fragmentShader);
	
			glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &rc);
			if (!rc) {
				glGetShaderInfoLog(fragmentShader, sizeof(log), NULL, log);
				printf("ERROR: Fragment Shader Compilation Failed\n%s\n", log);
				return 1;
			}
		}

		MAB_WRAP("link program") {
			if (program) glDeleteProgram(program);
			program = glCreateProgram();
			glAttachShader(program, vertexShader);
			glAttachShader(program, fragmentShader);
			glLinkProgram(program);
			
			glGetProgramiv(program, GL_LINK_STATUS, &rc);
			if(!rc){
				glGetProgramInfoLog(program, sizeof(log), NULL, log);
				printf("ERROR: Shader Program Linking Failed\n%s\n", log);
				return 1;
			}
			
			glUseProgram(program);
		}
	}
	__attribute__((fallthrough));

	case INIT_UNIFORMS:
	MAB_WRAP("init uniforms") {
		uTranslateX = glGetUniformLocation(program, "uTranslateX");
		uTranslateY = glGetUniformLocation(program, "uTranslateY");
		uScale = glGetUniformLocation(program, "uScale");
	}
	__attribute__((fallthrough));

	case INIT_GRAPH:
	MAB_WRAP("load graph") {
		MAB_WRAP("load node attributes") {
			char temp[512];
			snprintf(temp, sizeof(temp), "data/%s/nodes.dat", params->filename);

			f = fopen(temp, "r");
			fread(&ncount, sizeof(ncount), 1, f);

			nattribs = malloc(ncount * sizeof(*nattribs));

			for (i=0; i<ncount; ++i) {
				fread(&nattribs[i].size, sizeof(nattribs[i].size), 1, f);

				fread(&nattribs[i].type, sizeof(nattribs[i].type), 1, f);
				switch (nattribs[i].type) {
				case 'f': nattribs[i].type = GL_FLOAT; break;
				case 'i': nattribs[i].type = GL_INT: break;
				case 'u': nattribs[i].type = GL_UNSIGNED_INT; break;
				}

				fread(&nattribs[i].range, sizeof(nattribs[i].range), 1, f);

				nattribs[i].data = malloc(nattribs[i].size);
				fread(&nattribs[i].data, nattribs[i].size, 1, f);
			}
	}
	__attribute__((fallthrough));

	case INIT_ATTRIBUTES:
	MAB_WRAP("init node attributes") {
		for (i=0; i<ncount; ++i)
		MAB_WRAP("init buffer i=%d", i) {
			glGenBuffers(1, &aNodeBuffers[i]);
			glBindBuffer(GL_ARRAY_BUFFER, aNodeBuffers[i]);
			glBufferData(GL_ARRAY_BUFFER, nattribs[i].size, nattribs[i].data, GL_STATIC_DRAW);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}

		for (i=0; i<ncount; ++i)
		MAB_WRAP("init attribute i=%d", i) {
			char temp[32];
			snprintf(temp, sizeof(temp), "aNode%d", i);

			GLenum type;
			switch (nattribs[i].type) {
			case A_FLOAT32: type = GL_FLOAT; break;
			case A_INT32: type = GL_INT; break;
			case A_UINT32: type = GL_UNSIGNED_INT; break;
			default: fprintf(stderr, "bad type: %d\n", nattribs[i].type); exit(1);
			}

			aNodeLocations[i] = glGetAttribLocation(program, temp);
			glBindBuffer(GL_ARRAY_BUFFER, aNodeBuffers[i]);
			glVertexAttribPointer(aNodeLocations[i], 1, type, GL_FALSE, 0, nattribs[i].data);
			glEnableVertexAttribArray(aNodeLocations[i]);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
	}
	__attribute__((fallthrough));
	
	case INIT_INDEX:
	MAB_WRAP("init index buffer") {
		glGenBuffers(1, &indexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, edges->size, edges->data, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	__attribute__((fallthrough));

	case RENDER:
	MAB_WRAP("render") {
		glUniform1f(uTranslateX, -1.0f * x);
		glUniform1f(uTranslateY, -1.0f * y);
		glUniform1f(uScale, powf(2.0f, z));
		
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
		glDrawElements(GL_LINES, 2 * nedges, GL_UNSIGNED_INT, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glFlush();
	}
}
