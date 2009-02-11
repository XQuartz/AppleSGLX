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
#ifndef APPLE_GLX_PBUFFER_H
#define APPLE_GLX_PBUFFER_H

#include <stdbool.h>
#include "GL/glx.h"
#include "apple_cgl.h"

/* Returns true if an error occurred. */
bool apple_glx_pbuffer_create(Display *dpy, GLXFBConfig config, 
			      int width, int height, int *errorcode,
			      GLXPbuffer *pbuf);
void apple_glx_pbuffer_destroy(Display *dpy, GLXPbuffer pbuf);

/* Returns true if the drawable has a valid pbuffer object result. */
bool apple_glx_pbuffer_get(GLXDrawable d, CGLPBufferObj *result); 

/* These return true if the drawable is a valid Pbuffer: */
bool apple_glx_pbuffer_get_width(GLXDrawable d, int *width);
bool apple_glx_pbuffer_get_height(GLXDrawable d, int *height);
bool apple_glx_pbuffer_get_fbconfig_id(GLXDrawable d, XID *id);

/* Returns true if an error occurred. */
bool apple_glx_pbuffer_get_max_size(int *widthresult, int *heightresult);

#endif
