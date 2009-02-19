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
#ifndef APPLE_GLX_PIXMAP_H
#define APPLE_GLX_PIXMAP_H

#include <stdbool.h>
#include <limits.h>
#include "GL/glx.h"

/* mode is a __GLcontextModes * */
/* Returns true if an error occurred. */
bool apple_glx_pixmap_create(Display *dpy, int screen, Pixmap pixmap,
			     const void *mode);

void apple_glx_pixmap_destroy(Display *dpy, Pixmap pixmap);

bool apple_glx_is_pixmap(Display *dpy, GLXDrawable drawable);

/* Returns true if the pixmap is valid, and there is data for it. */
bool apple_glx_pixmap_data(Display *dpy, GLXPixmap pixmap, int *width,
			   int *height, int *pitch, int *bpp, void **ptr,
			   void **contextptr);

bool apple_glx_pixmap_query(GLXPixmap pixmap, int attribute,
			    unsigned int *value);

#endif
