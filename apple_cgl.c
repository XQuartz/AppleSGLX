/*
 Copyright (c) 2008 Apple Inc.
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation files
 (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge,
 publish, distribute, sublicense, and/or sell copies of the Software,
 and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT.  IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT
 HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
 
 Except as contained in this notice, the name(s) of the above
 copyright holders shall not be used in advertising or otherwise to
 promote the sale, use or other dealings in this Software without
 prior written authorization.
*/

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

#include "apple_cgl.h"

#ifndef OPENGL_FRAMEWORK_PATH
#define OPENGL_FRAMEWORK_PATH "/System/Library/Frameworks/OpenGL.framework/OpenGL"
#endif

static void *dl_handle = NULL;

struct apple_cgl_api apple_cgl;

static bool initialized = false;

static void *sym(void *h, const char *name) {
    void *s;

    s = dlsym(h, name);

    if(NULL == s) {
	fprintf(stderr, "error: %s\n", dlerror());
	abort();
    }

    return s;
}

void apple_cgl_init(void) {
    void *h;
    
    if(initialized)
	return;

    (void)dlerror(); /*drain dlerror*/
    h = dlopen(OPENGL_FRAMEWORK_PATH, RTLD_NOW);

    if(NULL == h) {
	fprintf(stderr, "error: unable to dlopen " OPENGL_FRAMEWORK_PATH " : " "%s\n",
		dlerror());
	abort();
    }

    dl_handle = h;

    apple_cgl.choose_pixel_format = sym(h, "CGLChoosePixelFormat");
    apple_cgl.destroy_pixel_format = sym(h, "CGLDestroyPixelFormat");

    apple_cgl.clear_drawable = sym(h, "CGLClearDrawable");
    apple_cgl.flush_drawable = sym(h, "CGLFlushDrawable");

    apple_cgl.create_context = sym(h, "CGLCreateContext");
    apple_cgl.destroy_context = sym(h, "CGLDestroyContext");

    apple_cgl.set_current_context = sym(h, "CGLSetCurrentContext");
    apple_cgl.get_current_context = sym(h, "CGLGetCurrentContext");
    apple_cgl.error_string = sym(h, "CGLErrorString");

    apple_cgl.set_off_screen = sym(h, "CGLSetOffScreen");
    
    apple_cgl.copy_context = sym(h, "CGLCopyContext");

    initialized = true;
}

void *apple_cgl_get_dl_handle(void) {
    return dl_handle;
}
