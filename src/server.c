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

	if (!render_ctx) {
		render_ctx = malloc(sizeof(*render_ctx));
		render_init(render_ctx);
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

static void
initialize(void) {
	index_response = MAB_create_response_from_file("static/index.html");
	error_response = MAB_create_response_from_file("static/error.html");
}

int
main(int argc, char **argv) {
	int opt_port;
	char *s;
	struct MHD_Daemon *daemon;

	opt_port = (s = getenv("FG_PORT")) ? atoi(s) : 8874;
	
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
