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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include "apple_glx_pixmap.h"
#include "appledri.h"

struct apple_glx_pixmap {
    GLXPixmap xpixmap;
    void *buffer;
    int width, height, pitch, /*bytes per pixel*/ bpp;
    size_t size;
    char path[PATH_MAX];
    int fd;
    struct apple_glx_pixmap *next, *previous;
};

static pthread_mutex_t pixmap_lock = PTHREAD_MUTEX_INITIALIZER;
static struct apple_glx_pixmap *pixmap_list = NULL;

static void lock_pixmap_list(void) {
    int err;
    
    err = pthread_mutex_lock(&pixmap_lock);
    
    if(err) {
        fprintf(stderr, "pthread_mutex_lock failure in %s: %d\n",
                __func__, err);
        abort();
    }
}

static void unlock_pixmap_list(void) {
    int err;
    
    err = pthread_mutex_unlock(&pixmap_lock);
    
    if(err) {
        fprintf(stderr, "pthread_mutex_unlock failure in %s: %d\n",
                __func__, err);
        abort();
    }    
}

static bool find_pixmap(GLXPixmap pix, struct apple_glx_pixmap **result) {
    struct apple_glx_pixmap *p;
    
    for(p = pixmap_list; p; p = p->next) {
	if(p->xpixmap == pix) {
	    *result = p;
	    return true;
	}
    }

    return false;
}


/* Return true if an error occurred. */
bool apple_glx_pixmap_create(Display *dpy, int screen, Pixmap pixmap) {
    struct apple_glx_pixmap *p;

    p = malloc(sizeof(*p));

    p->xpixmap = pixmap;
    p->buffer = NULL;

    if(!XAppleDRICreatePixmap(dpy, screen, pixmap,
			      &p->width, &p->height, &p->pitch, &p->bpp,
			      &p->size, p->path, PATH_MAX)) {
	free(p);
	return true;
    }

    /*printf("%s width %d height %d\n", __func__, p->width, p->height);
     */
      
    p->fd = shm_open(p->path, O_RDWR, 0);
    
    if(p->fd < 0) {
	perror("shm_open");
	XAppleDRIDestroyPixmap(dpy, pixmap);
	free(p);
	return true;
    }

    p->buffer = mmap(NULL, p->size, PROT_READ | PROT_WRITE,
		     MAP_FILE | MAP_SHARED, p->fd, 0);

    if(MAP_FAILED == p->buffer) {
	perror("mmap");
	XAppleDRIDestroyPixmap(dpy, pixmap);
	shm_unlink(p->path);
	free(p);
	return true;
    }

    lock_pixmap_list();
    
    p->previous = NULL;
    
    p->next = pixmap_list;
    
    if(pixmap_list)
	pixmap_list->previous = p;

    pixmap_list = p;    
    
    unlock_pixmap_list();    

    return false;
}

void apple_glx_pixmap_destroy(Display *dpy, GLXPixmap pixmap) {
    struct apple_glx_pixmap *p;

    lock_pixmap_list();

    if(find_pixmap(pixmap, &p)) {
	XAppleDRIDestroyPixmap(dpy, pixmap);
	munmap(p->buffer, p->size);
	close(p->fd);
        shm_unlink(p->path);
	free(p);

	if(p->previous) {
	    p->previous->next = p->next;
	} else {
	    pixmap_list = p->next;
	}

	if(p->next)
	    p->next->previous = p->previous;
    }

    unlock_pixmap_list();
}

bool apple_glx_is_pixmap(Display *dpy, GLXDrawable drawable) {
    struct apple_glx_pixmap *p;
    bool result = false;

    lock_pixmap_list();

    if(find_pixmap(drawable, &p))
	result = true;

    unlock_pixmap_list();

    return result;
}

bool apple_glx_pixmap_data(Display *dpy, GLXPixmap pixmap, int *width,
			   int *height, void **ptr) {
    struct apple_glx_pixmap *p;
    bool result = false;

    lock_pixmap_list();

    if(find_pixmap(pixmap, &p)) {
	*width = p->width;
	*height = p->height;
	*ptr = p->buffer;
	result = true;
    }

    unlock_pixmap_list();

    return result;    
}

