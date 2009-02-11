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
#include <assert.h>
#include <pthread.h>
#include "apple_glx.h"
#include "apple_glx_context.h"
#include "apple_glx_drawable.h"
#include "apple_glx_pbuffer.h"
#include "appledri.h"

static pthread_mutex_t drawables_lock = PTHREAD_MUTEX_INITIALIZER;
static struct apple_glx_drawable *drawables = NULL;

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
    for(i = drawables; i; i = i->next) {
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


void apple_glx_reference_drawable(struct apple_glx_drawable *agd) {
    agd->lock(agd);
    agd->reference_count++;
    agd->unlock(agd);
}

void apple_glx_release_drawable(struct apple_glx_drawable *agd) {
    agd->lock(agd);
    agd->reference_count--;
    agd->unlock(agd);
}

bool apple_glx_create_drawable(Display *dpy,
			       struct apple_glx_context *ac,
			       GLXDrawable drawable, 
			       CGLPBufferObj pbuf,
			       struct apple_glx_drawable **agdResult) {
    struct apple_glx_drawable *agd;
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
    agd->pbuffer_obj = pbuf;

    err = pthread_mutex_init(&agd->mutex, NULL);

    agd->lock = drawable_lock;
    agd->unlock = drawable_unlock;

    if(err) {
	fprintf(stderr, "pthread_mutex_init error: %d\n", err);
	abort();
    }

    agd->width = -1;
    agd->height = -1;
    agd->row_bytes = 0;
    agd->path[0] = '\0';
    agd->fd = -1;
    agd->buffer = NULL;
    agd->buffer_length = 0;
    
    agd->previous = NULL;

    if(!agd->pbuffer_obj && create_surface(dpy, ac, agd)) {
	free(agd);
	return true;
    }

    lock_drawables_list();
       
    /* Link the new drawable into the global list. */
    agd->next = drawables;

    if(drawables)
	drawables->previous = agd;

    drawables = agd;

    unlock_drawables_list();

    *agdResult = agd;

    return false;
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
	drawables = agd->next;
    }

    if(agd->next)
	agd->next->previous = agd->previous;

    /*
     * Only destroy the surface if it wasn't a pbuffer. 
     *
     * The pbuffer should be cleaned up explicitly with glXDestroyPbuffer.
     */
    if(!agd->pbuffer_obj) {
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
bool apple_glx_destroy_drawable(struct apple_glx_drawable *agd) {
    bool result;

    lock_drawables_list();

    result = destroy_drawable(agd);

    unlock_drawables_list();

    return result;
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
    

    if(NULL == drawables)
	return;

    XSync(dpy, False);

    old_handler = XSetErrorHandler(error_handler);

    lock_drawables_list();

    for(d = drawables; d;) {
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
