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
#include <limits.h>
#include <assert.h>
#include <pthread.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <OpenGL/CGLTypes.h>
#include <OpenGL/CGLCurrent.h>
#include <OpenGL/OpenGL.h>

#include "glxclient.h"

#include "apple_glx_context.h"
#include "appledri.h"
#include "apple_visual.h"
#include "apple_cgl.h"
#include "apple_glx_drawable.h"
#include "apple_glx_pbuffer.h"

static pthread_mutex_t context_lock = PTHREAD_MUTEX_INITIALIZER;

/*
 * This should be locked on creation and destruction of the 
 * apple_glx_contexts.
 *
 * It's also locked when the surface_notify_handler is searching
 * for a uid associated with a surface.
 */
static struct apple_glx_context *context_list = NULL;

/* This guards the context_list above. */
static void lock_context_list(void) {
    int err;

    err = pthread_mutex_lock(&context_lock);

    if(err) {
	fprintf(stderr, "pthread_mutex_lock failure in %s: %d\n",
		__func__, err);
	abort();
    }
}

static void unlock_context_list(void) {
    int err;

    err = pthread_mutex_unlock(&context_lock);

    if(err) {
	fprintf(stderr, "pthread_mutex_unlock failure in %s: %d\n",
		__func__, err);
	abort();
    }
}

static bool is_context_valid(struct apple_glx_context *ac) {
    struct apple_glx_context *i;

    lock_context_list();
    
    for(i = context_list; i; i = i->next) {
	if(ac == i) {
	    unlock_context_list();
	    return true;
	}
    }
    
    unlock_context_list();
    
    return false;
}

/* This creates an apple_private_context struct.  
 *
 * It's typically called to save the struct in a GLXContext.
 *
 * This is also where the CGLContextObj is created, and the CGLPixelFormatObj.
 */
bool apple_glx_create_context(void **ptr, Display *dpy, int screen, 
			      const void *mode, void *sharedContext,
			      int *errorptr, bool *x11errorptr) {
    struct apple_glx_context *ac;
    struct apple_glx_context *sharedac = sharedContext;
    CGLError error;

    *ptr = NULL;

    ac = malloc(sizeof *ac);

    if(NULL == ac) {
	*errorptr = BadAlloc;
	*x11errorptr = true;
	return true;
    }

    if(sharedac && !is_context_valid(sharedac)) {
	*errorptr = GLXBadContext;
	*x11errorptr = false;
	return true;
    }
    
    ac->context_obj = NULL;
    ac->pixel_format_obj = NULL;
    ac->drawable = NULL;
    ac->thread_id = pthread_self();
    ac->screen = screen;
    ac->double_buffered = false;
    ac->is_current = false;
    ac->made_current = false;
    
    apple_visual_create_pfobj(&ac->pixel_format_obj, mode, 
			      &ac->double_buffered, /*offscreen*/ false);
    
    error = apple_cgl.create_context(ac->pixel_format_obj, 
				     sharedac ? sharedac->context_obj : NULL,
				     &ac->context_obj);

    
    if(error) {
	(void)apple_cgl.destroy_pixel_format(ac->pixel_format_obj);		

	free(ac);
	
	if(kCGLBadMatch == error) {
	    *errorptr = BadMatch;
	    *x11errorptr = true;
	} else {
	    *errorptr = GLXBadContext;
	    *x11errorptr = false;
	}

	if(getenv("LIBGL_DIAGNOSTIC"))
	    fprintf(stderr, "error: %s\n", apple_cgl.error_string(error));
	
	return true;
    }

    /* The context creation succeeded, so we can link in the new context. */
    lock_context_list();

    if(context_list)
	context_list->previous = ac;

    ac->previous = NULL;
    ac->next = context_list;
    context_list = ac;

    unlock_context_list();

    *ptr = ac;

    return false;
}

void apple_glx_destroy_context(void **ptr, Display *dpy) {
    struct apple_glx_context *ac = *ptr;

    if(NULL == ac)
	return;

    if(apple_cgl.get_current_context() == ac->context_obj) {
	if(apple_cgl.set_current_context(NULL)) {
	    abort();
	}
    }

    /* Remove ac from the context_list as soon as possible. */
    lock_context_list();

    if(ac->previous) {
	ac->previous->next = ac->next;
    } else {
	context_list = ac->next;
    }

    if (ac->next) {
	ac->next->previous = ac->previous;
    }

    unlock_context_list();


    if(apple_cgl.clear_drawable(ac->context_obj)) {
	fprintf(stderr, "error: while clearing drawable!\n");
	abort();
    }
    
    /*
     * This causes surface_notify_handler to be called in apple_glx.c... 
     * We can NOT have a lock held at this point.  It would result in 
     * an abort due to an attempted deadlock.  This is why we earlier
     * removed the ac pointer from the double-linked list.
     */
    if(ac->drawable) {
	Drawable drawable = ac->drawable->drawable;

	/* 
	 * Release the drawable, so that the ->destroy may actually destroy
	 * the drawable, and we don't leak memory.  If the destroy returns
	 * false, then another context has a reference to the drawable.
	 */
	ac->drawable->release(ac->drawable);
	
	if(ac->drawable->destroy(ac->drawable)) {
	    /* 
	     * The drawable has no more references, so we can destroy
	     * the surface. 
	     */
	   XAppleDRIDestroySurface(dpy, ac->screen, drawable);
	}
    }

    if(apple_cgl.destroy_pixel_format(ac->pixel_format_obj)) {
	fprintf(stderr, "error: destroying pixel format in %s\n", __func__);
	abort();
    }

    if(apple_cgl.destroy_context(ac->context_obj)) {
	fprintf(stderr, "error: destroying context_obj in %s\n", __func__);
	abort();
    }
    
    free(ac);

    *ptr = NULL;
    
    apple_glx_garbage_collect_drawables(dpy);
}

#if 0
static bool setup_drawable(struct apple_glx_drawable *agd) {
    printf("buffer path %s\n", agd->path);
      
    agd->fd = shm_open(agd->path, O_RDWR, 0);

    if(-1 == agd->fd) {
	perror("open");
	return true;
    }
        
    agd->row_bytes = agd->width * /*TODO don't hardcode 4*/ 4;
    agd->buffer_length = agd->row_bytes * agd->height;

    printf("agd->width %d agd->height %d\n", agd->width, agd->height);

    agd->buffer = mmap(NULL, agd->buffer_length, PROT_READ | PROT_WRITE,
		       MAP_FILE | MAP_SHARED, agd->fd, 0);

    if(MAP_FAILED == agd->buffer) {
	perror("mmap");
	close(agd->fd);
	agd->fd = -1;
	return true;
    }

    return false;
}
#endif

static void update_viewport_and_scissor(Display *dpy, GLXDrawable drawable) {
    Window root;
    int x, y;
    unsigned int width, height, bd, depth;

    XGetGeometry(dpy, drawable, &root, &x, &y, &width, &height, &bd, &depth);

    glViewport(0, 0, width, height);
    glScissor(0, 0, width, height);
}

/* Return true if an error occured. */
bool apple_glx_make_current_context(Display *dpy, void *oldptr, void *ptr,
				    GLXDrawable drawable) {
    struct apple_glx_context *oldac = oldptr;
    struct apple_glx_context *ac = ptr;
    xp_error error;
    struct apple_glx_drawable *newagd = NULL;
    CGLError cglerr;
    bool same_drawable = false;

    assert(NULL != dpy);

    /* Reset the is_current state of the old context, if non-NULL. */
    if(oldac)
	oldac->is_current = false;

    if(NULL == ac) {
	/*Clear the current context.*/
	apple_cgl.set_current_context(NULL);
	
	return false;
    }
    
    if(None == drawable) {
	/* Clear the current drawable for this context_obj. */

	if(apple_cgl.set_current_context(ac->context_obj))
	    return true;
	
	if(apple_cgl.clear_drawable(ac->context_obj))
	    return true;
	
	return false;
    }

    newagd = apple_glx_find_drawable(dpy, drawable);

    if(ac->drawable == newagd)
	same_drawable = true;
    
    /*
     * Release and try to destroy the old drawable, so long as the new one
     * isn't the old. 
     */
    if(ac->drawable && !same_drawable) {
	ac->drawable->release(ac->drawable);
	ac->drawable->destroy(ac->drawable);
	ac->drawable = NULL;
    }
    
    if(NULL == newagd) {
	if(apple_glx_create_drawable(dpy, ac, drawable, &newagd))
	    return true;
    
	/* Save the new drawable with the context structure. */
	ac->drawable = newagd;

	/* Save a reference to the new drawable. */
	ac->drawable->reference(ac->drawable);
    } else {
	/* We are reusing an existing drawable structure. */

	if(!same_drawable) {
	    /* The drawable isn't the same as the previously made current. */
	    ac->drawable = newagd;
	    ac->drawable->reference(ac->drawable);
	}
    }

    cglerr = apple_cgl.set_current_context(ac->context_obj);

    if(kCGLNoError != cglerr) {
	fprintf(stderr, "set current error: %s\n",
		apple_cgl.error_string(cglerr));
	return true;
    }

    ac->is_current = true;

    assert(NULL != ac->context_obj);
    assert(NULL != ac->drawable);

    switch(ac->drawable->type) {
    case APPLE_GLX_DRAWABLE_PBUFFER: {
	CGLPBufferObj pbufobj;

	if(false == apple_glx_pbuffer_get(ac->drawable->drawable, &pbufobj)) {
	    fprintf(stderr, "internal error: drawable is a pbuffer, "
		    "but the pbuffer layer was unable to retrieve "
		    "the CGLPBufferObj!\n");
	    return true;
	}
		
	cglerr = apple_cgl.set_pbuffer(ac->context_obj, 
				       pbufobj,
				       0, 0, 0);
	    
	if(kCGLNoError != cglerr) {
	    fprintf(stderr, "set_pbuffer: %s\n", apple_cgl.error_string(cglerr));
	    return true;
	}
    }
	break;
	
    case APPLE_GLX_DRAWABLE_SURFACE:
	error = xp_attach_gl_context(ac->context_obj, ac->drawable->surface_id);

	if(error) {
	    fprintf(stderr, "error: xp_attach_gl_context returned: %d\n",
		    error);
	    return true;
	}
    
    
	if(!ac->made_current) {
	    /* 
	     * The first time a new context is made current the glViewport
	     * and glScissor should be updated.
	     */
	    update_viewport_and_scissor(dpy, ac->drawable->drawable);
	    ac->made_current = true;
	}
	break;

    case APPLE_GLX_DRAWABLE_PIXMAP: {
	int width, height, pitch, bpp;
	void *ptr;
	CGLContextObj ctxobj;
	void *ctxobjptr;
	
	apple_cgl.clear_drawable(ac->context_obj);

	if(false == apple_glx_pixmap_data(dpy, ac->drawable->drawable,
					  &width, &height, &pitch, &bpp,
					  &ptr, &ctxobjptr)) {
	    return true;
	}

	ctxobj = ctxobjptr;
	
	cglerr = apple_cgl.set_current_context(ctxobj);

	if(kCGLNoError != cglerr) {
	    fprintf(stderr, "set current context: %s\n",
		    apple_cgl.error_string(cglerr));
	    
	    return true;
	}

	cglerr = apple_cgl.set_off_screen(ctxobj, width, height,
					  pitch, ptr);

	if(kCGLNoError != cglerr) {
	    fprintf(stderr, "set off screen: %s\n",
		    apple_cgl.error_string(cglerr));
	    
	    return true;
	}
    }
	break;
    }
    

    ac->thread_id = pthread_self();

    return false;
}

bool apple_glx_is_current_drawable(void *ptr, GLXDrawable drawable) {
    struct apple_glx_context *ac = ptr;

    assert(NULL != ac);
    
    return (ac->drawable->drawable == drawable);
}

/* Return true if an error occurred. */
bool apple_glx_get_surface_from_uid(unsigned int uid, xp_surface_id *sid,
				    CGLContextObj *contextobj) {
    struct apple_glx_context *ac;

    lock_context_list();

    for(ac = context_list; ac; ac = ac->next) {
	if(ac->drawable && ac->drawable->uid == uid) {
	    *sid = ac->drawable->surface_id;
	    *contextobj = ac->context_obj;
	    unlock_context_list();
	    return false;
	}
    }

    unlock_context_list();

    return true;
}

bool apple_glx_copy_context(void *currentptr, void *srcptr, void *destptr, 
			    unsigned long mask, int *errorptr,
			    bool *x11errorptr) {
    struct apple_glx_context *src, *dest;
    CGLError err;

    src = srcptr;
    dest = destptr;

    if(src->screen != dest->screen) {
	*errorptr = BadMatch;
	*x11errorptr = true;
	return true;
    }
    
    if(dest == currentptr || dest->is_current) {
	*errorptr = BadAccess;
	*x11errorptr = true;
	return true;
    }

    /* 
     * If srcptr is the current context then we should do an implicit glFlush.
     */
    if(currentptr == srcptr)
	glFlush();
    
    err = apple_cgl.copy_context(src->context_obj, dest->context_obj, 
				 (GLbitfield)mask);
    
    if(kCGLNoError != err) {
	*errorptr = GLXBadContext;
	*x11errorptr = false;
	return true;
    }
    
    return false;
}

void apple_glx_destroy_drawable_in_any(Display *dpy, GLXDrawable d) {
    struct apple_glx_context *ac;
    struct apple_glx_drawable *agd;

    lock_context_list();

    for(ac = context_list; ac; ac = ac->next) {
	if(ac->drawable && ac->drawable->drawable == d) {
	    agd = ac->drawable;
	    ac->drawable = NULL;
	    agd->release(agd);
	    (void)agd->destroy(agd);
	    apple_cgl.clear_drawable(ac->context_obj);
	}
    }

    unlock_context_list();
}
