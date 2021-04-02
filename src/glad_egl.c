/*

    EGL loader generated by glad 0.1.34 on Fri Apr  2 14:26:10 2021.

    Language/Generator: C/C++
    Specification: egl
    APIs: egl=1.5
    Profile: -
    Extensions:
        
    Loader: True
    Local files: True
    Omit khrplatform: False
    Reproducible: False

    Commandline:
        --api="egl=1.5" --generator="c" --spec="egl" --local-files --extensions=""
    Online:
        https://glad.dav1d.de/#language=c&specification=egl&loader=on&api=egl%3D1.5
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glad/glad_egl.h"

int gladLoadEGL(void) {
    return gladLoadEGLLoader((GLADloadproc)eglGetProcAddress);
}

static int find_extensionsEGL(void) {
	return 1;
}

static void find_coreEGL(void) {
}

int gladLoadEGLLoader(GLADloadproc load) {
	(void) load;
	find_coreEGL();

	if (!find_extensionsEGL()) return 0;
	return 1;
}

