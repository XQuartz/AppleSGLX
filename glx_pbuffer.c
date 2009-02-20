/*
 * (C) Copyright IBM Corporation 2004
 * Copyright (C) 2009 Apple Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * IBM AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

/**
 * \file glx_pbuffer.c
 * Implementation of pbuffer related functions.
 * 
 * \author Ian Romanick <idr@us.ibm.com>
 */

#include <inttypes.h>
#include "glxclient.h"
#include <X11/extensions/extutil.h>
#include <X11/extensions/Xext.h>
#include <assert.h>
#include <string.h>
//#include "glapi.h"
#include "glxextensions.h"
#include "glcontextmodes.h"
//#include "glheader.h"

#include "apple_glx_pbuffer.h"
#include "apple_glx_pixmap.h"

/**
 * Create a new pbuffer.
 */
PUBLIC GLXPbuffer
glXCreatePbuffer(Display *dpy, GLXFBConfig config, const int *attrib_list) {
    GLXContext gc = __glXGetCurrentContext();
    int i, width, height;
    GLXPbuffer result;
    int errorcode;
    
    width = 0;
    height = 0;
    
    for(i = 0; attrib_list[i]; ++i) {
	switch(attrib_list[i]) {
	case GLX_PBUFFER_WIDTH:
	    width = attrib_list[i + 1];
	    ++i;
	    break;
	    
	case GLX_PBUFFER_HEIGHT:
	    height = attrib_list[i + 1];
	    ++i;
	    break;
	    
	case GLX_LARGEST_PBUFFER:
	    /* This is a hint we should probably handle, but how? */
	    break;

	case GLX_PRESERVED_CONTENTS:
	    /* The contents are always preserved with AppleSGLX with CGL. */
	    break;
	    
	default:
	    return None;
	}
    }
    
    if(apple_glx_pbuffer_create(dpy, config, width, height, &errorcode, 
				&result)) {
	xError error;
	
	LockDisplay(dpy);
	
	error.errorCode = errorcode;
	error.resourceID = 0;
	error.sequenceNumber = dpy->request;
	error.type = X_Error;
	error.majorCode = gc->majorOpcode;
	error.minorCode = X_GLXCreatePbuffer;
	_XError(dpy, &error);
	
	UnlockDisplay(dpy);
	
	return None;
    }
    
   return result;
}


/**
 * Destroy an existing pbuffer.
 */
PUBLIC void
glXDestroyPbuffer(Display *dpy, GLXPbuffer pbuf)
{
    apple_glx_pbuffer_destroy(dpy, pbuf);
}


/**
 * Query an attribute of a drawable.
 */
PUBLIC void
glXQueryDrawable(Display *dpy, GLXDrawable drawable,
		 int attribute, unsigned int *value) {
    GLXContext gc = __glXGetCurrentContext();
    Window root;
    int x, y;
    unsigned int width, height, bd, depth;
    xError error;


    if(apple_glx_pixmap_query(drawable, attribute, value))
	return; /*done*/

    if(apple_glx_pbuffer_query(drawable, attribute, value))
	return; /*done*/

    if(XGetGeometry(dpy, drawable, &root, &x, &y, &width, &height, &bd, &depth)) {
	switch(attribute) {
	case GLX_WIDTH:
	    *value = width;
	    return;

	case GLX_HEIGHT:
	    *value = height;
	    return;
	}
	/*FALL THROUGH*/
    }
   
    LockDisplay(dpy);
    
    error.errorCode = GLXBadDrawable;
    error.resourceID = 0;
    error.sequenceNumber = dpy->request;
    error.type = X_Error;
    error.majorCode = gc->majorOpcode;
    error.minorCode = X_GLXGetDrawableAttributes;
    _XError(dpy, &error);
	
    UnlockDisplay(dpy);
}


/**
 * Select the event mask for a drawable.
 */
PUBLIC void
glXSelectEvent(Display *dpy, GLXDrawable drawable, unsigned long mask)
{
    GLXContext gc = __glXGetCurrentContext();
    xError error;
    XWindowAttributes xwattr;
    
    if(apple_glx_pbuffer_set_event_mask(drawable, mask))
	return; /*done*/

    /* 
     * The spec allows a window, but currently there are no valid
     * events for a window, so do nothing.
     */
    if(XGetWindowAttributes(dpy, drawable, &xwattr))
	return; /*done*/

    /* The drawable seems to be invalid.  Report an error. */

    LockDisplay(dpy);
    
    error.errorCode = GLXBadDrawable;
    error.resourceID = 0;
    error.sequenceNumber = dpy->request;
    error.type = X_Error;
    error.majorCode = (gc) ? gc->majorOpcode : 0;
    error.minorCode = X_GLXChangeDrawableAttributes;
    _XError(dpy, &error);
        
    UnlockDisplay(dpy);
}


/**
 * Get the selected event mask for a drawable.
 */
PUBLIC void
glXGetSelectedEvent(Display *dpy, GLXDrawable drawable, unsigned long *mask)
{
    GLXContext gc = __glXGetCurrentContext();
    xError error;
    XWindowAttributes xwattr;
    
    if(apple_glx_pbuffer_get_event_mask(drawable, mask))
	return; /*done*/

    /* 
     * The spec allows a window, but currently there are no valid
     * events for a window, so do nothing, but set the mask to 0.
     */
    if(XGetWindowAttributes(dpy, drawable, &xwattr)) {
	/* The window is valid, so set the mask to 0.*/
	*mask = 0;
	return; /*done*/
    }

    /* The drawable seems to be invalid.  Report an error. */

    LockDisplay(dpy);
    
    error.errorCode = GLXBadDrawable;
    error.resourceID = 0;
    error.sequenceNumber = dpy->request;
    error.type = X_Error;
    error.majorCode = (gc) ? gc->majorOpcode : 0;
    error.minorCode = X_GLXChangeDrawableAttributes;
    _XError(dpy, &error);
        
    UnlockDisplay(dpy);
}


PUBLIC GLXPixmap
glXCreatePixmap( Display *dpy, GLXFBConfig config, Pixmap pixmap,
		 const int *attrib_list )
{
    const __GLcontextModes *modes = (const __GLcontextModes *)config;

    if(apple_glx_pixmap_create(dpy, modes->screen, pixmap, modes))
	return None;

    return pixmap;
}


PUBLIC GLXWindow
glXCreateWindow( Display *dpy, GLXFBConfig config, Window win,
		 const int *attrib_list )
{
    fprintf(stderr, "%s FIXME", __func__);
    abort();
}


PUBLIC void
glXDestroyPixmap(Display *dpy, GLXPixmap pixmap)
{
    apple_glx_pixmap_destroy(dpy, pixmap);
}


PUBLIC void
glXDestroyWindow(Display *dpy, GLXWindow win)
{
    fprintf(stderr, "%s FIXME\n", __func__);
    abort();
}

