#include "log.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <uuid/uuid.h>
#include <stdarg.h>
#include <stdlib.h>
//#include <strings.h>
#include <string.h>

static int _initialized = 0;
static pthread_mutex_t *_lock;
static pthread_cond_t *_cond;
static FILE *_file;
static int _always_flush;
static __thread int _t_initialized = 0;
static __thread int _t_continued;
static __thread int *_t_levels;
static __thread int _t_nlevels;
static __thread uuid_t _t_task;

static void die(char *message, ...) {
	va_list ap;

	va_start(ap, message);
	fprintf(stderr, "Error: ");
	vfprintf(stderr, message, ap);
	fprintf(stderr, "\n");
	va_end(ap);

	exit(1);
}

static char *formatTask(uuid_t task) {
	static __thread char task_str[37];
	static __thread uuid_t last_task;

	if (last_task == task) {
		return task_str;
	}

	uuid_unparse_lower(task, task_str);
	return task_str;
}

static char *formatLevel(int *levels, int nlevels) {
	static __thread char level_str[256];
	static __thread int last_nlevels;
	static __thread int last_level;
	int i, j;

	if (last_nlevels == nlevels && last_level == levels[nlevels-1]) {
		return level_str;
	}

	j = 0;
	for (i=0; i<nlevels; ++i) {
		if (j > sizeof(level_str)) die("overflow level_str");
		j += snprintf(level_str + j, sizeof(level_str) - j, "/%d", levels[i]);
	}

	last_nlevels = nlevels;
	last_level = levels[nlevels-1];
	return level_str;
}

static void writeLine(char *line) {
	if (!_initialized) return;
	fprintf(_file, "%s\n", line);
	if (_always_flush) {
		fflush(_file);
	}
}

static void writeLog(uuid_t task, int *levels, int nlevels, struct timeval *tv, char *key, char *value) {
	char *task_str, *level_str;

	if (!_initialized) return;
	if (!_t_initialized) return;

	task_str = formatTask(task);
	level_str = formatLevel(levels, nlevels);

	fprintf(_file, "%s%s\t%ld.%06ld\t%s\t%s\n", task_str, level_str, tv->tv_sec, tv->tv_usec, key, value);
}

static void flushLogs(void) {
	if (!_initialized) return;
	if (!_t_initialized) return;

	fflush(_file);
}

static void threadInitialize(void) {
	if (_t_initialized) return;
	_t_levels = malloc(256 * sizeof(*_t_levels));
	_t_nlevels = 0;
	_t_initialized = 1;
	_t_continued = 0;
}

int mabLogToFile(char *filename, char *mode) {
	FILE *file;

	if (_initialized) {
		die("logger already initialized");
	}

	file = fopen(filename, mode);
	if (!file) {
		return 1;
	}

	_lock = malloc(sizeof(*_lock));
	pthread_mutex_init(_lock, NULL);

	_cond = malloc(sizeof(*_cond));
	pthread_cond_init(_cond, NULL);

	_file = file;

	_initialized = 1;

	return 0;
}

void mabLogFlush(int always_flush) {
	_always_flush = always_flush;
}

void mabLogAction(char *message, ...) {
	struct timeval tv;
	char value[1024];
	va_list ap;

	if (!_t_initialized) threadInitialize();
	
	gettimeofday(&tv, NULL);

	va_start(ap, message);
	vsnprintf(value, sizeof(value), message, ap);
	va_end(ap);

	if (_t_nlevels == 0) {
		uuid_generate(_t_task);
		_t_nlevels = 1;
		_t_levels[0] = 1;
	} else {
		if (!_t_continued) _t_levels[_t_nlevels-1]++;
		else _t_continued = 0;
		_t_nlevels++;
		_t_levels[_t_nlevels-1] = 1;
	}
	
	pthread_mutex_lock(_lock);
	writeLog(_t_task, _t_levels, _t_nlevels, &tv, "@started", value);
	pthread_mutex_unlock(_lock);
}

void mabLogMessage(char *name, char *message, ...) {
	struct timeval tv;
	char value[1024];
	va_list ap;

	if (!_t_initialized) threadInitialize();
	
	gettimeofday(&tv, NULL);

	va_start(ap, message);
	vsnprintf(value, sizeof(value), message, ap);
	va_end(ap);

	if (_t_continued) die("after continuing, start an action, not a log message");

	if (_t_nlevels == 0) {
		die("cannot log message, create an action first; name=%s; message=%s", name, message);
	} else {
		_t_levels[_t_nlevels-1]++;
	}
	
	pthread_mutex_lock(_lock);
	writeLog(_t_task, _t_levels, _t_nlevels, &tv, name, value);
	pthread_mutex_unlock(_lock);
}

void mabLogForward(char **info) {
	char *task_str, *level_str;
	int size;

	if (!_t_initialized) threadInitialize();

	if (_t_nlevels == 0) {
		die("cannot forward an empty task");
	} else {
		_t_levels[_t_nlevels-1]++;
		task_str = formatTask(_t_task);
		level_str = formatLevel(_t_levels, _t_nlevels);
	}

	size = snprintf(NULL, 0, "%s%s", task_str, level_str);
	*info = malloc(size+1);
	snprintf(*info, size+1, "%s%s", task_str, level_str);
}

void mabLogContinue(const char *const info) {
	static __thread char temp[37];
	int offset;
	char *found;

	if (!_t_initialized) threadInitialize();

	strncpy(temp, info, 36);
	temp[36] = '\0';
	if (uuid_parse(temp, _t_task)) die("could not parse task");
	_t_nlevels = 0;
	offset = 37;
	for (;;) {
		_t_nlevels++;
		_t_levels[_t_nlevels-1] = atoi(info + offset);
		found = index(info + offset, '/');
		if (found == NULL) break;
		offset = found - info + 1;
	}

	_t_continued = 1;
}

void mabLogEnd(char *message, ...) {
	struct timeval tv;
	char value[1024];
	va_list ap;

	if (!_t_initialized) threadInitialize();
	
	gettimeofday(&tv, NULL);

	va_start(ap, message);
	vsnprintf(value, sizeof(value), message, ap);
	va_end(ap);

	if (_t_nlevels == 0) {
		die("no task is active, cannot end");
	} else {
		_t_levels[_t_nlevels-1]++;
	}
	
	pthread_mutex_lock(_lock);
	writeLog(_t_task, _t_levels, _t_nlevels, &tv, "@finished", value);
	if (_t_nlevels == 1) {
		flushLogs();
	}
	pthread_mutex_unlock(_lock);

	_t_nlevels--;
}


void mabLogDirectly(char *line) {
	writeLine(line);
}
