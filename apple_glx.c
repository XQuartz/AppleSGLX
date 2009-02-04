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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <dlfcn.h>
#include "appledri.h"
#include "apple_glx.h"
#include "apple_glx_context.h"
#include "apple_cgl.h"
#include "apple_xgl_api.h"

static bool initialized = false;
static int dri_event_base = 0;

const GLuint __glXDefaultPixelStore[9] = { 0, 0, 0, 0, 0, 0, 0, 0, 1 };

#ifndef OPENGL_LIB_PATH
#define OPENGL_LIB_PATH "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#endif

static void *libgl_handle = NULL;

int apple_get_dri_event_base(void) {
    if(!initialized) {
	fprintf(stderr, "error: dri_event_base called before apple_init_glx!\n");
	abort();
    }
    return dri_event_base;
}

static void surface_notify_handler(Display *dpy, unsigned int uid, int kind) {
    xp_surface_id sid;
    CGLContextObj contextobj;
    
    if(apple_glx_get_surface_from_uid(uid, &sid, &contextobj)) {
	/* The surface was probably destroyed. */
	return;
    }

    switch(kind) {
    case AppleDRISurfaceNotifyDestroyed:
	apple_cgl.clear_drawable(contextobj);
	xp_destroy_surface(sid);
	break;

    case AppleDRISurfaceNotifyChanged:
	xp_update_gl_context(contextobj);
	break;
	
    default:
	fprintf(stderr, "unhandled kind of event: %d in %s\n", kind, __func__);
    }
}

xp_client_id apple_glx_get_client_id(void) {
    static xp_client_id id;

    if(0 == id) {
	if((XP_Success != xp_init(XP_IN_BACKGROUND)) ||
	   (Success != xp_get_client_id(&id))) {
	    return 0;
	}
    }

    return id;
}

/* Return true if an error occured. */
bool apple_init_glx(Display *dpy) {
    int eventBase, errorBase;
    int major, minor, patch;

    if(initialized)
	return false;

    if(getenv("LIBGL_DIAGNOSTIC")) {
	printf("initializing libGL in %s", __func__);
    }

    apple_cgl_init();
    apple_xgl_init_direct();
    libgl_handle = dlopen(OPENGL_LIB_PATH, RTLD_LAZY);
    (void)apple_glx_get_client_id();

    if(!XAppleDRIQueryExtension(dpy, &eventBase, &errorBase))
        return true;
    
    if(!XAppleDRIQueryVersion(dpy, &major, &minor, &patch))
        return true;
  
    XAppleDRISetSurfaceNotifyHandler(surface_notify_handler);


    

    dri_event_base = eventBase;
    initialized = true;

    return false;
}

void apple_glx_swap_buffers(void *ptr) {
    struct apple_glx_context *ac = ptr;
    assert(NULL != ac);

    glFlush();

#if 0    
    if(XAppleDRISwapBuffers(ac->apple_glx_drawable->display,
			    ac->screen, ac->apple_glx_drawable->drawable))
	return;
#endif
    
    apple_cgl.flush_drawable(ac->context_obj);
}

void *apple_glx_get_proc_address(const GLubyte *procname) {
    size_t len;
    void *h, *s;
    char *pname = (char *)procname;

    assert(NULL != procname);
    len = strlen(pname);
   
    if(len < 3) {
	return NULL;
    }

    if((pname != strstr(pname, "glX")) && 
       (pname != strstr(pname, "gl"))) {
	fprintf(stderr, "warning: get proc address request is not for a gl or glX function");
	return NULL;
    }

    /* Search using the default symbols first. */
    (void)dlerror(); /*drain dlerror*/
    h = dlopen(NULL, RTLD_NOW);
    if(NULL == h) {
	fprintf(stderr, "warning: get proc address: %s\n", dlerror());
	return NULL;
    }
    
    s = dlsym(h, pname);

    if(NULL == s) {
	/* Try the libGL.dylib from the OpenGL.framework. */
	s = dlsym(libgl_handle, pname);
    }
    
    return s;
}

void apple_glx_waitx(Display *dpy, void *ptr) {
    struct apple_private_context *ac = ptr;

    (void)ac;

    glFlush();
    glFinish();
    XSync(dpy, False);
}
