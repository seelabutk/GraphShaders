#include "server.h"
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <microhttpd.h>
#include <zlib.h>
#include <pthread.h>
#include <zorder.h>

#include "fg.h"
#include "render.h"
#include "base64.h"
#include "shaders/default.frag.h"
#include "shaders/default.vert.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


struct MHD_Response *
MAB_create_response_from_file(const char *filename) {
	FILE *f;
	size_t size, actual;
	char *data;

	f = fopen(filename, "rb");
	if (f == NULL) {
		fprintf(stderr, "could not open %s\n", filename);
		//exit(1);
		return NULL;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	rewind(f);
	
	data = malloc(size);
	actual = fread(data, 1, size, f);
	if (actual != size) {
		fprintf(stderr, "didn't read all of %s\n", filename);
		//exit(1);
		free(data);
		fclose(f);
		return NULL;
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

ANSWER(Index) {
	struct MHD_Response *index_response;
	int rc;

	index_response = MAB_create_response_from_file("static/index.html");
	if(index_response == NULL){
		printf("Could not read file static/index.html\n");
		exit(1);
	}
	rc = MHD_queue_response(conn, MHD_HTTP_OK, index_response);
	MHD_destroy_response(index_response);
	return rc;
}

struct MHD_Response *error_response;
ANSWER(Error) {
	fprintf(stderr, "404: '%s' '%s'\n", method, url);
	return MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, error_response);	
}

ANSWER(Static) {
	struct MHD_Response *response;
	int rc;

	const char *URL = url+1;

	response = MAB_create_response_from_file(URL);
	if(response == NULL){
		rc = Error(cls, conn, url, method, version, data, size, con_cls);
	}else{
		rc = MHD_queue_response(conn, MHD_HTTP_OK, response);
		MHD_destroy_response(response);
	}
	return rc;
}

static char *last_graph_name = NULL;
static char *last_vert = NULL;
static char *last_frag = NULL;
static int last_z = -1;
static ZOrderStruct *last_z_zo = NULL;
static int last_res = -1;
static struct render_ctx *render_ctx = NULL;
static ZOrderStruct *last_res_zo = NULL;
static int pbufferWidth = 256;
static int pbufferHeight = 256;
ANSWER(Tile) {
	int x;
	int y;
	int z;
	char *dataset, *options;
	int rc;
	if (5 != (rc = sscanf(url, "/tile/%m[^/,]%m[^-]-%d-%d-%d", &dataset, &options, &z, &x, &y))) {
		fprintf(stderr, "rc: %d\n", rc);
		fprintf(stderr, "options: %s\n", options);
		return MHD_queue_response(conn, MHD_HTTP_NOT_FOUND, error_response);	
		exit(1);
	}

	char *s;

	char *opt_vert = strdup(src_shaders_default_vert);
	char *opt_frag = strdup(src_shaders_default_frag);
	int opt_res = 1;
	float opt_rc_start = (s = getenv("FG_RC_START")) ? atof(s) : 0.0;
	float opt_rc_end = (s = getenv("FG_RC_END")) ? atof(s) : 1.0;

	char *it = options + 1;
	for (;;) {
		char *key = it;

		it = strchrnul(it, ',');
		if (!*it) break;
		*it++ = '\0';

		char *value = it;
		
		it = strchrnul(it, ',');
		int done = *it == '\0';
		*it++ = '\0';

		if (strncmp(value, "base64:", strlen("base64:")) == 0) {
			char *data = value + strlen("base64:");
			size_t len = strlen(data);

			size_t outlen = len + 128;
			char *out = malloc(outlen + 1);
			if (base64decode(data, len, out, &outlen) != 0) {
				fprintf(stderr, "ERROR: base64decode failed\n");
				return MHD_NO;
			}

			out[outlen-1] = '\0';
			value = out;
		}

		if (strcmp(key, "vert") == 0) {
			opt_vert = value;
		} else if (strcmp(key, "frag") == 0) {
			opt_frag = value;
		} else if (strcmp(key, "res") == 0) {
			opt_res = atoi(value);
		} else if (strcmp(key, "rc-start") == 0) {
			opt_rc_start = atof(value);
		} else if (strcmp(key, "rc-end") == 0) {
			opt_rc_end = atof(value);
		} else {
			fprintf(stderr, "WARNING: unhandled option '%s' = '%s'\n", key, value);
		}

		if (done) break;
	}

	if (!render_ctx) {
		fprintf(stderr, "First initialization of OpenGL\n");
		render_ctx = malloc(sizeof(*render_ctx));
		render_ctx->pbufferWidth = pbufferWidth;
		render_ctx->pbufferHeight = pbufferHeight;
		
		render_preinit(render_ctx);
	}

	int max_xy = (int)pow(2, z);
	if (last_z != z) {
		last_z_zo = zoInitZOrderIndexing(max_xy, max_xy, 0, ZORDER_LAYOUT);
		last_z = z;
	}

	if (0 <= x && x < max_xy && 0 <= y && y < max_xy) {
		float z_rc_index = (zoGetIndex(last_z_zo, x, y, 0)) / (float)last_z_zo->zo_totalSize;
		if (!(opt_rc_start <= z_rc_index && z_rc_index <= opt_rc_end)) {
			fprintf(stderr, "Redirecting! ! (%f <= %f <= %f)\n", opt_rc_start, z_rc_index, opt_rc_end);
			static void *data = "Found";
			size_t size = strlen(data);

			struct MHD_Response *response;
			response = MHD_create_response_from_buffer(size, data, MHD_RESPMEM_PERSISTENT);
			MHD_add_response_header(response, "Location", url);
			int rc = MHD_queue_response(conn, 302, response);
			MHD_destroy_response(response);
			return rc;
		}
	}

	if (last_graph_name == NULL || strcmp(last_graph_name, dataset) != 0 ||
	    last_vert == NULL || strcmp(last_vert, opt_vert) != 0 ||
	    last_frag == NULL || strcmp(last_frag, opt_frag) != 0 ||
	    last_res != opt_res) {
		fprintf(stderr, "Loading dataset '%s'\n", dataset);
		char nodespath[PATH_MAX], edgespath[PATH_MAX];
		snprintf(nodespath, PATH_MAX, "data/%s/nodes.csv", dataset);
		snprintf(edgespath, PATH_MAX, "data/%s/edges.csv", dataset);

		struct graph *graph;
		graph = malloc(sizeof(*graph));
		load(nodespath, edgespath, graph);

		render_init(render_ctx, graph, NULL, 0, opt_vert, opt_frag);

		zoInitZOrderIndexing(opt_res, opt_res, 0, ZORDER_LAYOUT);

		if (last_graph_name) free(last_graph_name);
		last_graph_name = strdup(dataset);

		if (last_vert) free(last_vert);
		last_vert = strdup(opt_vert);

		if (last_frag) free(last_frag);
		last_frag = strdup(opt_frag);

		last_res = opt_res;
	}
	free(opt_vert);
	free(opt_frag);
	free(dataset);

	

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
	MHD_add_response_header(response, "Connection", "close");
	
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
	{ "GET", "/static/", 0, Static },
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
	error_response = MAB_create_response_from_file("static/error.html");
	if(error_response == NULL){
		printf("Could not read file static/error.html");
		exit(1);
	}
}

int
main(int argc, char **argv) {
	int opt_port;
	char *s, *opt_bind;
	struct MHD_Daemon *daemon;
	struct sockaddr_in addr;

	opt_port = (s = getenv("FG_PORT")) ? atoi(s) : 8889;
	opt_bind = (s = getenv("FG_BIND")) ? s : "0.0.0.0";
	
	fprintf(stderr, "Listening on %s:%d\n", opt_bind, opt_port);
	
	initialize();

	memset(&addr, sizeof(addr), 0);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(opt_port);
	inet_aton(opt_bind, &addr.sin_addr);
	
	daemon = MHD_start_daemon(MHD_USE_SELECT_INTERNALLY, 0, NULL, NULL, &answer, NULL, MHD_OPTION_SOCK_ADDR, &addr, MHD_OPTION_END);
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
