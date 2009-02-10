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
#include "apple_glx_pbuffer.h"

struct apple_glx_pbuffer {
    GLXPbuffer xid; /* our pixmap */
    int width, height;
    CGLPBufferObj buffer_obj;
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


bool apple_glx_pbuffer_create(Display *dpy, GLXFBConfig config, 
			      int width, int height, GLXPbuffer *pbufResult) {
    CGLError err;
    struct apple_glx_pbuffer *pbuf;
    Window root;
    int screen;

    pbuf = malloc(sizeof(*pbuf));
    if(NULL == pbuf) {
	/*FIXME set BadAlloc.*/
	return true;
    }

    pbuf->width = width;
    pbuf->height = height;
    pbuf->previous = NULL;
    pbuf->next = NULL;

    err = apple_cgl.create_pbuffer(width, height, GL_TEXTURE_RECTANGLE_EXT,
		     GL_RGBA, 0, &pbuf->buffer_obj);

    if(kCGLNoError != err) {
	free(pbuf);
	/*FIXME Fill in BadMatch and so on.*/
	return true;
    }

    root = DefaultRootWindow(dpy);
    screen = DefaultScreen(dpy);

    pbuf->xid = XCreatePixmap(dpy, root, (unsigned int)width,
			      (unsigned int)height,
			      DefaultDepth(dpy, screen));

    if(None == pbuf->xid) {
	apple_cgl.destroy_pbuffer(pbuf->buffer_obj);
	free(pbuf);
	return true;
    } 

    *pbufResult = pbuf->xid;

    lock_list();

    pbuf->next = pbuffer_list;
    
    if(pbuffer_list)
	pbuffer_list->previous = pbuf;

    unlock_list();
    
    return false;
}

void apple_glx_pbuffer_destroy(Display *dpy, GLXPbuffer xid) {
    struct apple_glx_pbuffer *pbuf;

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

bool apple_glx_pbuffer_get(GLXDrawable d, CGLPBufferObj *result) {
    struct apple_glx_pbuffer *pbuf;

    lock_list();

    for(pbuf = pbuffer_list; pbuf; pbuf = pbuf->next) {
	if(pbuf->xid == d) {
	    *result = pbuf->buffer_obj;
	    unlock_list();
	    return true;
	}
    }

    unlock_list();

    return false;
}
