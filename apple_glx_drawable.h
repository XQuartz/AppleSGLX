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

#ifndef APPLE_GLX_DRAWABLE_H
#define APPLE_GLX_DRAWABLE_H

#include <pthread.h>
#include <stdbool.h>
#include <limits.h>
#include <GL/glx.h>
#define XP_NO_X_HEADERS
#include <Xplugin.h>
#undef XP_NO_X_HEADERS
#include "apple_glx_pixmap.h"

enum {
    APPLE_GLX_DRAWABLE_SURFACE = 1,
    APPLE_GLX_DRAWABLE_PBUFFER,
    APPLE_GLX_DRAWABLE_PIXMAP
};

struct apple_glx_drawable {
    Display *display;
    int reference_count;
    GLXDrawable drawable;
    xp_surface_id surface_id;
    unsigned int uid;
    int type; /* APPLE_GLX_DRAWABLE_* */

    /* 
     * This mutex protects the reference count and any other drawable data.
     * It's used to prevent an early release of a drawable.
     */
    pthread_mutex_t mutex;
    void (*lock)(struct apple_glx_drawable *agd);
    void (*unlock)(struct apple_glx_drawable *agd);

    void (*reference)(struct apple_glx_drawable *agd);
    void (*release)(struct apple_glx_drawable *agd);
    
    bool (*destroy)(struct apple_glx_drawable *agd);

    bool (*is_pbuffer)(struct apple_glx_drawable *agd);

    bool (*is_pixmap)(struct apple_glx_drawable *agd);

/*BEGIN These are used for the mixed mode drawing... */
    int width, height;
    int row_bytes;
    char path[PATH_MAX];
    int fd; /* The file descriptor for this drawable's shared memory. */
    void *buffer; /* The memory for the drawable.  Typically shared memory. */
    size_t buffer_length;
/*END*/

    struct apple_glx_drawable *previous, *next;
};

struct apple_glx_context;

/* May return NULL if not found */
struct apple_glx_drawable *apple_glx_find_drawable(Display *dpy, 
						   GLXDrawable drawable);

/* Returns true on error */
bool apple_glx_create_drawable(Display *dpy, 
			       struct apple_glx_context *ac,
			       GLXDrawable drawable,
			       struct apple_glx_drawable **agd);

void apple_glx_garbage_collect_drawables(Display *dpy);

void apple_glx_destroy_drawable_in_any(Display *dpy, GLXDrawable d);

/* 
 * This returns the total number of drawables. 
 * It's mostly intended for debugging and introspection.
 */
unsigned int apple_glx_get_drawable_count(void);

#endif
