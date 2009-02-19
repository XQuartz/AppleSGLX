/*
 Copyright (c) 2008, 2009 Apple Inc.
 
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
#include <assert.h>
#include <pthread.h>
#include "apple_glx.h"
#include "apple_glx_context.h"
#include "apple_glx_drawable.h"
#include "apple_glx_pbuffer.h"
#include "appledri.h"

static pthread_mutex_t drawables_lock = PTHREAD_MUTEX_INITIALIZER;
static struct apple_glx_drawable *drawables_list = NULL;

static void lock_drawables_list(void) {
    int err;
    
    err = pthread_mutex_lock(&drawables_lock);

    if(err) {
	fprintf(stderr, "pthread_mutex_lock failure in %s: %d\n",
		__func__, err);
        abort();
    }
}

static void unlock_drawables_list(void) {
    int err;
    
    err = pthread_mutex_unlock(&drawables_lock);

    if(err) {
	fprintf(stderr, "pthread_mutex_unlock failure in %s: %d\n",
		__func__, err);
        abort();
    }
}

struct apple_glx_drawable *apple_glx_find_drawable(Display *dpy,
						   GLXDrawable drawable) {
    struct apple_glx_drawable *i, *agd = NULL;

    lock_drawables_list();

    /*
     * Pixmaps aren't required to have a globally unique ID from what I recall.
     * so we use the display connection with the drawable lookup.
     */
    for(i = drawables_list; i; i = i->next) {
	if(i->drawable == drawable) {
	    agd = i;
	    break;
	}
    }
    
    unlock_drawables_list();

    return agd;
}


/* Return true if an error occured. */
static bool create_surface(Display *dpy, struct apple_glx_context *ac,
			   struct apple_glx_drawable *agd) {
    unsigned int key[2];
    xp_client_id id;

    id = apple_glx_get_client_id();
    if(0 == id)
	return true;

    assert(None != agd->drawable);

    if(XAppleDRICreateSurface(dpy, ac->screen, agd->drawable,
			      id, key, &agd->uid)) {
	xp_error error;

	error = xp_import_surface(key, &agd->surface_id);

	if(error) {
	    fprintf(stderr, "error: xp_import_surface returned: %d\n",
		    error);
	    return true;
	}

	return false;
    }

    return true;
}

static void drawable_lock(struct apple_glx_drawable *agd) {
    int err;

    err = pthread_mutex_lock(&agd->mutex);

    if(err) {
	fprintf(stderr, "pthread_mutex_lock error: %d\n", err);
	abort();
    }
}

static void drawable_unlock(struct apple_glx_drawable *agd) {
    int err;

    err = pthread_mutex_unlock(&agd->mutex);

    if(err) {
	fprintf(stderr, "pthread_mutex_unlock error: %d\n", err);
	abort();
    }
}


static void reference_drawable(struct apple_glx_drawable *agd) {
    agd->lock(agd);
    agd->reference_count++;
    agd->unlock(agd);
}

static void release_drawable(struct apple_glx_drawable *agd) {
    agd->lock(agd);
    agd->reference_count--;
    agd->unlock(agd);
}

/* The drawables list must be locked prior to calling this. */
/* Return true if the drawable was destroyed. */
static bool destroy_drawable(struct apple_glx_drawable *agd) {
    xp_error error;
    
    agd->lock(agd);

    if(agd->reference_count > 0) {
	agd->unlock(agd);
	return false;
    }

    agd->unlock(agd);
  
    if(agd->previous) {
	agd->previous->next = agd->next;
    } else {
	/*
	 * The item must be at the head of the list, if it
	 * has no previous pointer. 
	 */
	drawables_list = agd->next;
    }

    if(agd->next)
	agd->next->previous = agd->previous;

    
    if(APPLE_GLX_DRAWABLE_SURFACE == agd->type) {
	error = xp_destroy_surface(agd->surface_id);
	
	if(error) {
	    fprintf(stderr, "xp_destroy_surface error: %d\n", (int)error);
	}
    }
        
    free(agd);

    return true;
}

/* This delinks the drawable from the list. */
/*
 * This is typically called when a context is destroyed or 
 * garbage is collected below. 
 */
static bool destroy_drawable_callback(struct apple_glx_drawable *agd) {
    bool result;

    lock_drawables_list();

    result = destroy_drawable(agd);

    unlock_drawables_list();

    return result;
}

static bool is_pbuffer(struct apple_glx_drawable *agd) {
    return APPLE_GLX_DRAWABLE_PBUFFER == agd->type;
}

static bool is_pixmap(struct apple_glx_drawable *agd) {
    return APPLE_GLX_DRAWABLE_PIXMAP == agd->type;
}

bool apple_glx_create_drawable(Display *dpy,
			       struct apple_glx_context *ac,
			       GLXDrawable drawable, 
			       struct apple_glx_drawable **agdResult) {
    struct apple_glx_drawable *agd;
    CGLPBufferObj pbufobj;
    int err;
    
    *agdResult = NULL;

    agd = malloc(sizeof *agd);

    if(NULL == agd) {
	perror("malloc");
	return true;
    }

    agd->display = dpy;
    agd->reference_count = 0;
    agd->drawable = drawable;
    agd->surface_id = 0;
    agd->uid = 0;

    agd->type = APPLE_GLX_DRAWABLE_SURFACE;

    if(apple_glx_pbuffer_get(drawable, &pbufobj))
	agd->type = APPLE_GLX_DRAWABLE_PBUFFER;

    if(apple_glx_is_pixmap(dpy, drawable))
	agd->type = APPLE_GLX_DRAWABLE_PIXMAP;

    err = pthread_mutex_init(&agd->mutex, NULL);
    
    if(err) {
	fprintf(stderr, "pthread_mutex_init error: %d\n", err);
	abort();
    }

    agd->lock = drawable_lock;
    agd->unlock = drawable_unlock;

    agd->reference = reference_drawable;
    agd->release = release_drawable;

    agd->destroy = destroy_drawable_callback;

    agd->is_pbuffer = is_pbuffer;
    agd->is_pixmap = is_pixmap;

    agd->width = -1;
    agd->height = -1;
    agd->row_bytes = 0;
    agd->path[0] = '\0';
    agd->fd = -1;
    agd->buffer = NULL;
    agd->buffer_length = 0;
    
    agd->previous = NULL;

    if(APPLE_GLX_DRAWABLE_SURFACE == agd->type 
       && create_surface(dpy, ac, agd)) {
	free(agd);
	return true;
    }

    lock_drawables_list();
       
    /* Link the new drawable into the global list. */
    agd->next = drawables_list;

    if(drawables_list)
	drawables_list->previous = agd;

    drawables_list = agd;

    unlock_drawables_list();

    *agdResult = agd;

    return false;
}

static int error_count = 0;

static int error_handler(Display *dpy, XErrorEvent *err) {
    if(err->error_code == BadWindow) {
	++error_count;
    }

    return 0;
} 

void apple_glx_garbage_collect_drawables(Display *dpy) {
    struct apple_glx_drawable *d, *dnext;
    Window root;
    int x, y;
    unsigned int width, height, bd, depth;
    int (*old_handler)(Display *, XErrorEvent *);
    

    if(NULL == drawables_list)
	return;

    old_handler = XSetErrorHandler(error_handler);

    XSync(dpy, False);

    lock_drawables_list();

    for(d = drawables_list; d;) {
	dnext = d->next;

	d->lock(d);

	if(d->reference_count > 0) {
	    /* 
	     * Skip this, because some context still retains a reference 
	     * to the drawable.
	     */
	    d->unlock(d);
	    d = dnext;
	    continue;
	}

	d->unlock(d);

	error_count = 0;

	/* 
	 * Mesa uses XGetWindowAttributes, but some of these things are 
	 * most definitely not Windows, and that's against the rules.
	 * XGetGeometry on the other hand is legal with a Pixmap and Window.
	 */
	XGetGeometry(dpy, d->drawable, &root, &x, &y, &width, &height, &bd,
		     &depth);
	
	if(error_count > 0) {
	    /*
	     * Note: this may not actually destroy the drawable.
	     * If another context retains a reference to the drawable
	     * after the reference count test above. 
	     */
	    (void)destroy_drawable(d);
	    error_count = 0;
	}

	d = dnext;
    }

    XSetErrorHandler(old_handler);

    unlock_drawables_list();
}

unsigned int apple_glx_get_drawable_count(void) {
    unsigned int result = 0;
    struct apple_glx_drawable *d;

    lock_drawables_list();

    for(d = drawables_list; d; d = d->next)
	++result;
    
    unlock_drawables_list();

    return result;
}
