/*
 Copyright (c) 2009 Apple Inc.
 
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

#include <stdlib.h>
#include <pthread.h>
#include "glcontextmodes.h"
#include "apple_glx_pbuffer.h"
#include "apple_glx_drawable.h"

struct apple_glx_pbuffer {
    GLXPbuffer xid; /* our pixmap */
    int width, height;
    GLint fbconfigID;
    CGLPBufferObj buffer_obj;
    unsigned long event_mask;    
    struct apple_glx_pbuffer *previous, *next;
};

static struct apple_glx_pbuffer *pbuffer_list = NULL;

static pthread_mutex_t pbuffer_lock = PTHREAD_MUTEX_INITIALIZER;

static void lock_list(void) {
    int err;

    err = pthread_mutex_lock(&pbuffer_lock);

    if(err) {
	fprintf(stderr, "pthread_mutex_lock failure in %s: %d\n",
		__func__, err);
	abort();
    }
}

static void unlock_list(void) {
    int err;
    
    err = pthread_mutex_unlock(&pbuffer_lock);

    if(err) {
	fprintf(stderr, "pthread_mutex_unlock failure in %s: %d\n",
		__func__, err);
	abort();
    }
}

static bool find_pbuffer(GLXDrawable d, struct apple_glx_pbuffer **result) {
    struct apple_glx_pbuffer *pbuf;

    lock_list();

    for(pbuf = pbuffer_list; pbuf; pbuf = pbuf->next) {
	if(pbuf->xid == d) {
	    *result = pbuf;
	    unlock_list();
	    return true;
	}
    }
    
    unlock_list();

    return false;
}


bool apple_glx_pbuffer_create(Display *dpy, GLXFBConfig config, 
			      int width, int height, int *errorcode,
			      GLXPbuffer *result) {
    CGLError err;
    struct apple_glx_pbuffer *pbuf;
    Window root;
    int screen;
    __GLcontextModes *modes = (__GLcontextModes *)config;

    pbuf = malloc(sizeof(*pbuf));

    if(NULL == pbuf) {
	*errorcode = BadAlloc;
	return true;
    }

    pbuf->width = width;
    pbuf->height = height;
    pbuf->previous = NULL;
    pbuf->next = NULL;

    err = apple_cgl.create_pbuffer(width, height, GL_TEXTURE_RECTANGLE_EXT,
				   (modes->alphaBits > 0) ? GL_RGBA : GL_RGB,
				   0, &pbuf->buffer_obj);

    if(kCGLNoError != err) {
	free(pbuf);
	*errorcode = BadMatch;
	return true;
    }

    root = DefaultRootWindow(dpy);
    screen = DefaultScreen(dpy);

    /*
     * This pixmap is only used for a persistent XID.
     * The XC-MISC extension cleans up XIDs and reuses them transparently,
     * so we need to retain a server-side reference.
     */
    pbuf->xid = XCreatePixmap(dpy, root, (unsigned int)1,
			      (unsigned int)1,
			      DefaultDepth(dpy, screen));

    if(None == pbuf->xid) {
	apple_cgl.destroy_pbuffer(pbuf->buffer_obj);
	free(pbuf);
	*errorcode = BadAlloc;
	return true;
    } 

    pbuf->fbconfigID = modes->fbconfigID;

    pbuf->event_mask = 0;

    *result = pbuf->xid;

    /* Link the pbuffer into the list. */
    lock_list();

    pbuf->next = pbuffer_list;
    
    if(pbuffer_list)
	pbuffer_list->previous = pbuf;

    pbuffer_list = pbuf;

    unlock_list();
    
    return false;
}

void apple_glx_pbuffer_destroy(Display *dpy, GLXPbuffer xid) {
    struct apple_glx_pbuffer *pbuf;

    /*
     * This cleans up the drawable associated with any currently alive
     * context.
     */
    apple_glx_destroy_drawable_in_any(dpy, xid);
    
    lock_list();

    for(pbuf = pbuffer_list; pbuf; pbuf = pbuf->next) {
	if(pbuf->xid == xid) {
	    apple_cgl.destroy_pbuffer(pbuf->buffer_obj);
	    XFreePixmap(dpy, pbuf->xid);
	    
	    if(pbuf->previous) {
		pbuf->previous->next = pbuf->next;
	    } else {
		pbuffer_list = pbuf->next;
	    }

	    if(pbuf->next)
		pbuf->next->previous = pbuf->previous;
	    
	    free(pbuf);

	    break;
	}
    }

    unlock_list();    
}

/* Return true if able to get the pbuffer object for the drawable. */
bool apple_glx_pbuffer_get(GLXDrawable d, CGLPBufferObj *result) {
    struct apple_glx_pbuffer *pbuf;

    if(find_pbuffer(d, &pbuf)) {
	*result = pbuf->buffer_obj;
	return true;
    }

    return false;
}

/* Return true if an error occurred. */
static bool get_max_size(int *widthresult, int *heightresult) {
    CGLContextObj oldcontext;
    GLint ar[2];
   
    oldcontext = apple_cgl.get_current_context();

    if(!oldcontext) {
	/* 
	 * There is no current context, so we need to make one in order
	 * to call glGetInteger.
	 */
	CGLPixelFormatObj pfobj;
	CGLError err;
	CGLPixelFormatAttribute attr[10];
	int c = 0;
	GLint vsref = 0;
	CGLContextObj newcontext;
       
	attr[c++] = kCGLPFAColorSize;
	attr[c++] = 32;
	attr[c++] = 0;

	err = apple_cgl.choose_pixel_format(attr, &pfobj, &vsref);
	if(kCGLNoError != err) {
	    if(getenv("LIBGL_DIAGNOSTIC")) {
		printf("choose_pixel_format error in %s: %s\n", __func__,
		       apple_cgl.error_string(err));
	    }

	    return true;
	}
	    

	err = apple_cgl.create_context(pfobj, NULL, &newcontext);

	if(kCGLNoError != err) {
	    if(getenv("LIBGL_DIAGNOSTIC")) {
		printf("create_context error in %s: %s\n", __func__,
		       apple_cgl.error_string(err));
	    }

	    apple_cgl.destroy_pixel_format(pfobj);
	   
	    return true;
	}

	err = apple_cgl.set_current_context(newcontext);

	if (kCGLNoError != err) {
	    printf("set_current_context error in %s: %s\n", __func__,
		   apple_cgl.error_string(err));
	    return true;
	}

	
	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, ar);

	apple_cgl.set_current_context(oldcontext);
	apple_cgl.destroy_context(newcontext);
	apple_cgl.destroy_pixel_format(pfobj);	
    } else {
	/* We have a valid context. */

	glGetIntegerv(GL_MAX_VIEWPORT_DIMS, ar);
    }

    *widthresult = ar[0];
    *heightresult = ar[1];
        
    return false;
}

bool apple_glx_pbuffer_query(GLXPbuffer p, int attr, unsigned int *value) {
    bool result = false;
    struct apple_glx_pbuffer *pbuf;
    
    if(find_pbuffer(p, &pbuf)) {
	switch(attr) {
	case GLX_WIDTH:
	    *value = pbuf->width;
	    result = true;
	    break;
	    
	case GLX_HEIGHT:
	    *value = pbuf->height;
	    result = true;
	    break;

	case GLX_PRESERVED_CONTENTS:
	    *value = true;
	    result = true;
	    break;
	    
	case GLX_LARGEST_PBUFFER: {
	    int width, height;
	    if(get_max_size(&width, &height)) {
		fprintf(stderr, "internal error: "
			"unable to find the largest pbuffer!\n");
	    } else {
		*value = width;
		result = true;
	    }
	}
	    break;

	case GLX_FBCONFIG_ID:
	    *value = pbuf->fbconfigID;
	    result = true;
	    break;
	}
    }

    return result;
}

bool apple_glx_pbuffer_set_event_mask(GLXDrawable d, unsigned long mask) {
    struct apple_glx_pbuffer *pbuf;

    if(find_pbuffer(d, &pbuf)) {
	pbuf->event_mask = mask;
	return true;
    }

    return false;
}

bool apple_glx_pbuffer_get_event_mask(GLXDrawable d, unsigned long *mask) {
    struct apple_glx_pbuffer *pbuf;

    if(find_pbuffer(d, &pbuf)) {
	*mask = pbuf->event_mask;
	return true;
    }
    
    return false;
}
