#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <microhttpd.h>
#include <zlib.h>
#include <pthread.h>

#include <fg.h>
#include <render.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

enum MAB_CollectorStrategy {
	MAB_NONE_STRATEGY = 0,
	MAB_MULT_STRATEGY,
	MAB_INCR_STRATEGY,
};

struct MAB_Collector {
	char *data;
	size_t length;
	size_t _size;
	enum MAB_CollectorStrategy _strategy;
	size_t _arg;
};

struct MAB_Collector *
MAB_create_collector(size_t size, enum MAB_CollectorStrategy strategy, size_t arg) {
	struct MAB_Collector *coll;
	
	coll = malloc(sizeof(*coll));
	if (coll == NULL) {
		fprintf(stderr, "create_collector: could not alloc\n");
		return NULL;
	}
	
	coll->data = malloc(size);
	coll->_size = size;
	coll->length = 0;
	coll->_strategy = strategy;
	coll->_arg = arg;
	
	return coll;
}

int
MAB_collect(struct MAB_Collector *coll, const char *data, size_t size) {
	size_t newsize;
	char *newdata;
	
	while (coll->_size - coll->length < size) {
		if (coll->_strategy == MAB_NONE_STRATEGY) {
			newsize = coll->_size;
		} else if (coll->_strategy == MAB_MULT_STRATEGY) {
			newsize = coll->_size + (coll->_size * coll->_arg) / 100;
		} else if (coll->_strategy == MAB_INCR_STRATEGY) {
			newsize = coll->_size + coll->_arg;
		}
		
		if (newsize <= coll->_size) {
			return MHD_NO;
		}
		
		newdata = realloc(coll->data, newsize);
		if (newdata == NULL) {
			fprintf(stderr, "collect: could not realloc\n");
			return MHD_NO;
		}
		
		coll->_size = newsize;
		coll->data = newdata;
	}
	
	memcpy(coll->data + coll->length, data, size);
	coll->length += size;
	
	return MHD_YES;
}

int
MAB_destroy_collector(struct MAB_Collector *coll) {
	free(coll->data);
	coll->data = NULL;
	coll->length = 0;
	coll->_size = 0;
	coll->_strategy = MAB_NONE_STRATEGY;
	coll->_arg = 0;
	free(coll);
	
	return MHD_YES;
}

struct MHD_Response *
MAB_create_response_from_buffer_gzip(size_t size, void *data, enum MHD_ResponseMemoryMode mode, int level) {
	struct MHD_Response *response;
	int rv;
	Bytef *out;
	uLongf outlen;
	const Bytef *in;
	uLongf inlen;
	
	in = (const Bytef *)data;
	inlen = (uLongf)size;
	outlen = compressBound(inlen);
	out = malloc(outlen);
	rv = compress2(out, &outlen, in, inlen, level);
	if (rv != Z_OK) {
		fprintf(stderr, "compress2: %d\n", rv);
		return NULL;
	}
	
	response = MHD_create_response_from_buffer(outlen, out, MHD_RESPMEM_MUST_FREE);
	MHD_add_response_header(response, "Content-Encoding", "deflate");
	
	return response;
}

#define ANSWER(name) int name(void *cls, struct MHD_Connection *conn, const char *url, const char *method, const char *version, const char *data, size_t *size, void **con_cls)

struct MHD_Response *index_response;
ANSWER(Index) {
	return MHD_queue_response(conn, MHD_HTTP_OK, index_response);	
}

struct MHD_Response *error_response;
ANSWER(Error) {
	fprintf(stderr, "404: '%s' '%s'\n", method, url);
	return MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, error_response);	
}

static struct render_ctx *render_ctx = NULL;
ANSWER(Tile) {
	int x;
	int y;
	int z;
	if (3 != sscanf(url, "/tile/%d/%d/%d", &z, &x, &y)) {
		return MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, error_response);	
	}

	printf("Got: [%d][%d][%d]\n", x, y, z);
	printf("TID: %d\n", (int)pthread_self());

	if (!render_ctx) {
		char *argv[2] = { "foo", NULL };
		render_ctx = malloc(sizeof(*render_ctx));
		render_init(render_ctx, 1, argv);
	}

	void *output;
	size_t outputlen;

	{
	void *buffer;
	size_t bufferlen;

	buffer = NULL;
	bufferlen = 0;

	render_focus_tile(render_ctx, z, x, y);
	render_display(render_ctx);
	render_copy_to_buffer(render_ctx, &bufferlen, &buffer);

	void *jpg;
	size_t jpglen;
	size_t jpgsize;

	jpgsize = 1024;
	jpg = malloc(jpgsize);
	jpglen = 0;

	void mywriter(void *context, void *data, int size) {
		if (jpglen + size > jpgsize) {
			jpgsize = jpglen + size;
			jpg = realloc(jpg, jpgsize);
		}
		memcpy(jpg + jpglen, data, size);
		jpglen += size;
	}

	(void)stbi_write_jpg_to_func(mywriter, NULL, 256, 256, 3, buffer, 95);

	output = jpg;
	outputlen = jpglen;
	}
	
	struct MHD_Response *response;
	int ret;
	
	//response = MAB_create_response_from_buffer_gzip(outputlen, output, MHD_RESPMEM_MUST_FREE, 6);
	response = MHD_create_response_from_buffer(outputlen, output, MHD_RESPMEM_MUST_FREE);
	
	const char *origin = MHD_lookup_connection_value(conn, MHD_HEADER_KIND, "origin");
	if (origin != NULL) {
		MHD_add_response_header(response, "Access-Control-Allow-Origin", origin);
	}
	MHD_add_response_header(response, "Content-Type", "image/jpg");
	
	ret = MHD_queue_response(conn, MHD_HTTP_OK, response);
	MHD_destroy_response(response);
	
	return ret;
}

struct {
	const char *method;
	const char *url;
	int exact;
	MHD_AccessHandlerCallback cb;
} routes[] = {
	{ NULL, NULL, 0, Error },
	{ "GET", "/", 1, Index },
	{ "GET", "/tile/", 0, Tile },
	/* { "POST", "/collect/", 0, Collect }, */
};

ANSWER(answer) {
	size_t i;
	
	for (i=1; i<sizeof(routes)/sizeof(*routes); ++i) {
		if (strcmp(method, routes[i].method) != 0) continue;
		if (routes[i].exact && strcmp(url, routes[i].url) != 0) continue;
		if (!routes[i].exact && strncmp(url, routes[i].url, strlen(routes[i].url)) != 0) continue;
		
		return routes[i].cb(cls, conn, url, method, version, data, size, con_cls);
	}
	
	return routes[0].cb(cls, conn, url, method, version, data, size, con_cls);
}

void initialize(void);

int opt_port;

int
main(int argc, char **argv) {
	int i, fail;
	struct MHD_Daemon *daemon;
	
	printf("TID: %d\n", (int)pthread_self());

	fail = 0;
	opt_port = 8874;
	for (i=1; i<argc; ++i) {
		if (strcmp(argv[i], "--port") == 0) {
			opt_port = atoi(argv[++i]);
		} else {
			fprintf(stderr, "unknown argument '%s'\n", argv[i]);
			fail = 1;
		}
	}
	
	if (fail) {
		fprintf(stderr, "exiting due to bad arguments\n");
		return 1;
	}
	
	fprintf(stderr, "Listening on 0.0.0.0:%d\n", opt_port);
	
	initialize();
	
	daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, opt_port, NULL, NULL, &answer, NULL, MHD_OPTION_END);
	if (daemon == NULL) {
		fprintf(stderr, "could not start daemon\n");
		return 1;
	}
	
	fprintf(stderr, "press enter to stop\n");
	getchar();
	fprintf(stderr, "stopping...\n");
	
	MHD_stop_daemon(daemon);
	return 0;
}

struct MHD_Response *
MAB_create_response_from_file(char *filename) {
	FILE *f;
	size_t size, actual;
	char *data;
	
	f = fopen(filename, "rb");
	if (f == NULL) {
		fprintf(stderr, "could not open %s\n", filename);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	rewind(f);
	
	data = malloc(size);
	actual = fread(data, 1, size, f);
	if (actual != size) {
		fprintf(stderr, "didn't read all of %s\n", filename);
		exit(1);
	}
	fclose(f);
	
	return MHD_create_response_from_buffer(size, data, MHD_RESPMEM_MUST_FREE);
}

void
initialize(void) {
	index_response = MAB_create_response_from_file("static/index.html");
	error_response = MAB_create_response_from_file("static/error.html");
}
