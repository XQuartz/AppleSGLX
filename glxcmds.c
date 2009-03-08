/*
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 * Copyright (c) 2008, 2009 Apple Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 */

/**
 * \file glxcmds.c
 * Client-side GLX interface.
 */

#include "glxclient.h"
#include "glxextensions.h"
#include "glcontextmodes.h"

#include "apple_glx_context.h"
#include "apple_glx.h"
#include "glx_error.h"

static const char __glXGLXClientVendorName[] = "SGI";
static const char __glXGLXClientVersion[] = "1.4";


/****************************************************************************/

#ifdef GLX_DIRECT_RENDERING

static Bool windowExistsFlag;
static int windowExistsErrorHandler(Display *dpy, XErrorEvent *xerr)
{
    if (xerr->error_code == BadWindow) {
	windowExistsFlag = GL_FALSE;
    }
    return 0;
}

/**
 * Find drawables in the local hash that have been destroyed on the
 * server.
 * 
 * \param dpy    Display to destroy drawables for
 * \param screen Screen number to destroy drawables for
 */
static void GarbageCollectDRIDrawables(Display *dpy, __GLXscreenConfigs *sc)
{
    XID draw;
    __GLXDRIdrawable *pdraw;
    XWindowAttributes xwa;
    int (*oldXErrorHandler)(Display *, XErrorEvent *);

    /* Set no-op error handler so Xlib doesn't bail out if the windows
     * has alreay been destroyed on the server. */
    XSync(dpy, GL_FALSE);
    oldXErrorHandler = XSetErrorHandler(windowExistsErrorHandler);

    if (__glxHashFirst(sc->drawHash, &draw, (void *)&pdraw) == 1) {
	do {
	    windowExistsFlag = GL_TRUE;
	    XGetWindowAttributes(dpy, draw, &xwa); /* dummy request */
	    if (!windowExistsFlag) {
		/* Destroy the local drawable data, if the drawable no
		   longer exists in the Xserver */
		(*pdraw->destroyDrawable)(pdraw);
                __glxHashDelete(sc->drawHash, draw);
	    }
	} while (__glxHashNext(sc->drawHash, &draw, (void *)&pdraw) == 1);
    }

    XSync(dpy, GL_FALSE);
    XSetErrorHandler(oldXErrorHandler);
}

extern __GLXDRIdrawable *
GetGLXDRIDrawable(Display *dpy, GLXDrawable drawable, int * const scrn_num);

/**
 * Get the __DRIdrawable for the drawable associated with a GLXContext
 * 
 * \param dpy       The display associated with \c drawable.
 * \param drawable  GLXDrawable whose __DRIdrawable part is to be retrieved.
 * \param scrn_num  If non-NULL, the drawables screen is stored there
 * \returns  A pointer to the context's __DRIdrawable on success, or NULL if
 *           the drawable is not associated with a direct-rendering context.
 */
_X_HIDDEN __GLXDRIdrawable *
GetGLXDRIDrawable(Display *dpy, GLXDrawable drawable, int * const scrn_num)
{
    __GLXdisplayPrivate *priv = __glXInitialize(dpy);
    __GLXDRIdrawable *pdraw;
    const unsigned  screen_count = ScreenCount(dpy);
    unsigned   i;
    __GLXscreenConfigs *psc;

    if (priv == NULL)
	return NULL;
    
    for (i = 0; i < screen_count; i++) {
	psc = &priv->screenConfigs[i];
	if (psc->drawHash == NULL)
	    continue;

	if (__glxHashLookup(psc->drawHash, drawable, (void *) &pdraw) == 0) {
	    if (scrn_num != NULL)
		*scrn_num = i;
	    return pdraw;
	}
    }

    return NULL;
}

#endif


/**
 * Get the GLX per-screen data structure associated with a GLX context.
 * 
 * \param dpy   Display for which the GLX per-screen information is to be
 *              retrieved.
 * \param scrn  Screen on \c dpy for which the GLX per-screen information is
 *              to be retrieved.
 * \returns A pointer to the GLX per-screen data if \c dpy and \c scrn
 *          specify a valid GLX screen, or NULL otherwise.
 * 
 * \todo Should this function validate that \c scrn is within the screen
 *       number range for \c dpy?
 */

static __GLXscreenConfigs *
GetGLXScreenConfigs(Display *dpy, int scrn)
{
    __GLXdisplayPrivate * const priv = __glXInitialize(dpy);

    return (priv->screenConfigs != NULL) ? &priv->screenConfigs[scrn] : NULL;
}


static int
GetGLXPrivScreenConfig( Display *dpy, int scrn, __GLXdisplayPrivate ** ppriv,
			__GLXscreenConfigs ** ppsc )
{
    /* Initialize the extension, if needed .  This has the added value
     * of initializing/allocating the display private 
     */
    
    if ( dpy == NULL ) {
	return GLX_NO_EXTENSION;
    }

    *ppriv = __glXInitialize(dpy);
    if ( *ppriv == NULL ) {
	return GLX_NO_EXTENSION;
    }

    /* Check screen number to see if its valid */
    if ((scrn < 0) || (scrn >= ScreenCount(dpy))) {
	return GLX_BAD_SCREEN;
    }

    /* Check to see if the GL is supported on this screen */
    *ppsc = &((*ppriv)->screenConfigs[scrn]);
    if ( (*ppsc)->configs == NULL ) {
	/* No support for GL on this screen regardless of visual */
	return GLX_BAD_VISUAL;
    }

    return Success;
}


/**
 * Determine if a \c GLXFBConfig supplied by the application is valid.
 *
 * \param dpy     Application supplied \c Display pointer.
 * \param config  Application supplied \c GLXFBConfig.
 *
 * \returns If the \c GLXFBConfig is valid, the a pointer to the matching
 *          \c __GLcontextModes structure is returned.  Otherwise, \c NULL
 *          is returned.
 */
static __GLcontextModes *
ValidateGLXFBConfig( Display * dpy, GLXFBConfig config )
{
    __GLXdisplayPrivate * const priv = __glXInitialize(dpy);
    const unsigned num_screens = ScreenCount(dpy);
    unsigned   i;
    const __GLcontextModes * modes;


    if ( priv != NULL ) {
	for ( i = 0 ; i < num_screens ; i++ ) {
	    for ( modes = priv->screenConfigs[i].configs
		  ; modes != NULL
		  ; modes = modes->next ) {
		if ( modes == (__GLcontextModes *) config ) {
		    return (__GLcontextModes *) config;
		}
	    }
	}
    }

    return NULL;
}


/**
 * \todo It should be possible to move the allocate of \c client_state_private
 * later in the function for direct-rendering contexts.  Direct-rendering
 * contexts don't need to track client state, so they don't need that memory
 * at all.
 * 
 * \todo Eliminate \c __glXInitVertexArrayState.  Replace it with a new
 * function called \c __glXAllocateClientState that allocates the memory and
 * does all the initialization (including the pixel pack / unpack).
 */
static
GLXContext AllocateGLXContext( Display *dpy )
{
     GLXContext gc;
     int bufSize;
     CARD8 opcode;
    __GLXattribute *state;

    if (!dpy)
        return NULL;

    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return NULL;
    }

    /* Allocate our context record */
    gc = (GLXContext) Xmalloc(sizeof(struct __GLXcontextRec));
    if (!gc) {
	/* Out of memory */
	return NULL;
    }
    memset(gc, 0, sizeof(struct __GLXcontextRec));

    state = Xmalloc(sizeof(struct __GLXattributeRec));
    if (state == NULL) {
	/* Out of memory */
	Xfree(gc);
	return NULL;
    }
    gc->client_state_private = state;
    memset(gc->client_state_private, 0, sizeof(struct __GLXattributeRec));
    state->NoDrawArraysProtocol = (getenv("LIBGL_NO_DRAWARRAYS") != NULL);

    /*
    ** Create a temporary buffer to hold GLX rendering commands.  The size
    ** of the buffer is selected so that the maximum number of GLX rendering
    ** commands can fit in a single X packet and still have room in the X
    ** packet for the GLXRenderReq header.
    */

    bufSize = (XMaxRequestSize(dpy) * 4) - sz_xGLXRenderReq;
    gc->buf = (GLubyte *) Xmalloc(bufSize);
    if (!gc->buf) {
	Xfree(gc->client_state_private);
	Xfree(gc);
	return NULL;
    }
    gc->bufSize = bufSize;

    /* Fill in the new context */
    gc->renderMode = GL_RENDER;

    state->storePack.alignment = 4;
    state->storeUnpack.alignment = 4;

    gc->attributes.stackPointer = &gc->attributes.stack[0];

    /*
    ** PERFORMANCE NOTE: A mode dependent fill image can speed things up.
    ** Other code uses the fastImageUnpack bit, but it is never set
    ** to GL_TRUE.
    */
    gc->fastImageUnpack = GL_FALSE;
    gc->fillImage = __glFillImage;
    gc->pc = gc->buf;
    gc->bufEnd = gc->buf + bufSize;
    gc->isDirect = GL_FALSE;
    if (__glXDebug) {
	/*
	** Set limit register so that there will be one command per packet
	*/
	gc->limit = gc->buf;
    } else {
	gc->limit = gc->buf + bufSize - __GLX_BUFFER_LIMIT_SIZE;
    }
    gc->createDpy = dpy;
    gc->majorOpcode = opcode;

    /*
    ** Constrain the maximum drawing command size allowed to be
    ** transfered using the X_GLXRender protocol request.  First
    ** constrain by a software limit, then constrain by the protocl
    ** limit.
    */
    if (bufSize > __GLX_RENDER_CMD_SIZE_LIMIT) {
        bufSize = __GLX_RENDER_CMD_SIZE_LIMIT;
    }
    if (bufSize > __GLX_MAX_RENDER_CMD_SIZE) {
        bufSize = __GLX_MAX_RENDER_CMD_SIZE;
    }
    gc->maxSmallRenderCommandSize = bufSize;

    gc->apple = NULL;
    gc->do_destroy = False;

    return gc;
}


/**
 * Create a new context.  Exactly one of \c vis and \c fbconfig should be
 * non-NULL.
 * 
 * \param use_glx_1_3  For FBConfigs, should GLX 1.3 protocol or
 *                     SGIX_fbconfig protocol be used?
 * \param renderType   For FBConfigs, what is the rendering type?
 */

static GLXContext
CreateContext(Display *dpy, XVisualInfo *vis,
	      const __GLcontextModes * const fbconfig,
	      GLXContext shareList,
	      Bool allowDirect, GLXContextID contextID,
	      Bool use_glx_1_3, int renderType)
{
    GLXContext gc;
    int screen = (fbconfig == NULL) ? vis->screen : fbconfig->screen;
    __GLXscreenConfigs *const psc = GetGLXScreenConfigs(dpy, screen);
    const __GLcontextModes *mode;
    int errorcode;
    bool x11error;

    if ( dpy == NULL )
       return NULL;

    gc = AllocateGLXContext(dpy);
    if (!gc)
	return NULL;

    if (None == contextID) {
	if ( (vis == NULL) && (fbconfig == NULL) )
	    return NULL;

    }
    
    gc->xid = contextID;
    gc->imported = GL_FALSE;

    if(vis) {
	mode = _gl_context_modes_find_visual(psc->visuals, vis->visualid);
    } else {
	mode = _gl_context_modes_find_fbconfig(psc->configs, 
					       fbconfig->fbconfigID);
    }

    if(NULL == mode) {
	__glXSendError(dpy, BadValue, vis->visualid, X_GLXCreateContext,
		       true);
	__glXFreeContext(gc);

	return NULL;
    }
    
    
    if(apple_glx_create_context(&gc->apple, dpy, screen, mode, 
				shareList ? shareList->apple : NULL,
				&errorcode, &x11error)) {
	__glXSendError(dpy, errorcode, 0, X_GLXCreateContext, x11error);
	__glXFreeContext(gc);

	return NULL;
    }
    
    gc->currentContextTag = -1;
    gc->mode = mode;
    gc->isDirect = allowDirect;

    return gc;
}

PUBLIC GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis,
				   GLXContext shareList, Bool allowDirect)
{

   return CreateContext(dpy, vis, NULL, shareList, allowDirect, None,
			False, 0);
}

_X_HIDDEN void __glXFreeContext(__GLXcontext *gc)
{
    if (gc->vendor) XFree((char *) gc->vendor);
    if (gc->renderer) XFree((char *) gc->renderer);
    if (gc->version) XFree((char *) gc->version);
    if (gc->extensions) XFree((char *) gc->extensions);
    
    //__glFreeAttributeState(gc);
    XFree((char *) gc->buf);
    Xfree((char *) gc->client_state_private);
    XFree((char *) gc);
    
}

/*
** Destroy the named context
*/
static void 
DestroyContext(Display *dpy, GLXContext gc)
{
    __glXLock();

    if (gc->currentDpy) {
	/* 
	 * Set the Bool that indicates that we should destroy this GLX context
	 * when the context is no longer current.
	 */
	gc->do_destroy = True;
	__glXUnlock();
    } else {
	/* Destroy the handle if not current to anybody */
	__glXUnlock();

	if(gc->apple)
	    apple_glx_destroy_context(&gc->apple, dpy);

	__glXFreeContext(gc);
    }
}

PUBLIC void glXDestroyContext(Display *dpy, GLXContext gc)
{
    DestroyContext(dpy, gc);
}

/*
** Return the major and minor version #s for the GLX extension
*/
PUBLIC Bool glXQueryVersion(Display *dpy, int *major, int *minor)
{
    __GLXdisplayPrivate *priv;

    /* Init the extension.  This fetches the major and minor version. */
    priv = __glXInitialize(dpy);
    if (!priv) return GL_FALSE;

    if (major) *major = priv->majorVersion;
    if (minor) *minor = priv->minorVersion;

    return GL_TRUE;
}

/*
** Query the existance of the GLX extension
*/
PUBLIC Bool glXQueryExtension(Display *dpy, int *errorBase, int *eventBase)
{
    int major_op, erb, evb;
    Bool rv;

    rv = XQueryExtension(dpy, GLX_EXTENSION_NAME, &major_op, &evb, &erb);
    if (rv) {
	if (errorBase) *errorBase = erb;
	if (eventBase) *eventBase = evb;
    }
    return rv;
}

/*
** Put a barrier in the token stream that forces the GL to finish its
** work before X can proceed.
*/
PUBLIC void glXWaitGL(void)
{
    GLXContext gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;

    if (!dpy) return;

    /* Flush any pending commands out */
    __glXFlushRenderBuffer(gc, gc->pc);

    glFinish();
}

/*
** Put a barrier in the token stream that forces X to finish its
** work before GL can proceed.
*/
PUBLIC void glXWaitX(void)
{
    GLXContext gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;

    if (!dpy) return;

    /* Flush any pending commands out */
    __glXFlushRenderBuffer(gc, gc->pc);

    apple_glx_waitx(dpy, gc->apple);

    return;
}

PUBLIC void glXUseXFont(Font font, int first, int count, int listBase)
{
    GLXContext gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;

    if (!dpy) return;

    /* Flush any pending commands out */
    (void) __glXFlushRenderBuffer(gc, gc->pc);
    
    DRI_glXUseXFont(font, first, count, listBase);    
}

/************************************************************************/

/*
** Copy the source context to the destination context using the
** attribute "mask".
*/
PUBLIC void glXCopyContext(Display *dpy, GLXContext source,
			   GLXContext dest, unsigned long mask)
{
    GLXContext gc = __glXGetCurrentContext();
    int errorcode;
    bool x11error;

    if(apple_glx_copy_context(gc->apple, source->apple, dest->apple,
			      mask, &errorcode, &x11error)) {
	__glXSendError(dpy, errorcode, 0, X_GLXCopyContext, x11error);
    }
}

/**
 * \todo
 * Shouldn't this function \b always return \c GL_FALSE when
 * \c GLX_DIRECT_RENDERING is not defined?  Do we really need to bother with
 * the GLX protocol here at all?
 */
PUBLIC Bool glXIsDirect(Display *dpy, GLXContext gc)
{
    /*
     * This isn't an ideal test.  
     * glXIsDirect should probably search a list of contexts.
     */
    if(NULL == gc) {
	__glXSendError(dpy, GLXBadContext, 0, X_GLXIsDirect, false);
	return False;
    }

    return gc->isDirect;
}

PUBLIC GLXPixmap glXCreateGLXPixmap(Display *dpy, XVisualInfo *vis, 
				    Pixmap pixmap)
{
    int screen = vis->screen;
    __GLXscreenConfigs *const psc = GetGLXScreenConfigs(dpy, screen);
    const __GLcontextModes *modes;
    
    modes = _gl_context_modes_find_visual(psc->visuals, vis->visualid);

    if(apple_glx_pixmap_create(dpy, vis->screen, pixmap, modes))
	return None;

    return pixmap;
}

/*
** Destroy the named pixmap
*/
PUBLIC void glXDestroyGLXPixmap(Display *dpy, GLXPixmap glxpixmap)
{
    if(apple_glx_pixmap_destroy(dpy, glxpixmap))
	__glXSendError(dpy, GLXBadPixmap, glxpixmap, X_GLXDestroyPixmap, false);
}

PUBLIC void glXSwapBuffers(Display *dpy, GLXDrawable drawable) {
    GLXContext gc = glXGetCurrentContext();
    
    if(gc && apple_glx_is_current_drawable(dpy, gc->apple, drawable)) {
	apple_glx_swap_buffers(gc->apple);
    } else {
	__glXSendError(dpy, GLXBadCurrentWindow, 0, X_GLXSwapBuffers, false);
    }
}


/*
** Return configuration information for the given display, screen and
** visual combination.
*/
PUBLIC int glXGetConfig(Display *dpy, XVisualInfo *vis, int attribute,
			int *value_return)
{
    __GLXdisplayPrivate *priv;
    __GLXscreenConfigs *psc;
    __GLcontextModes *modes;
    int   status;

    status = GetGLXPrivScreenConfig( dpy, vis->screen, & priv, & psc );
    if ( status == Success ) {
	modes = _gl_context_modes_find_visual(psc->visuals, vis->visualid);

	/* Lookup attribute after first finding a match on the visual */
	if ( modes != NULL ) {
	    return _gl_get_context_mode_data( modes, attribute, value_return );
	}
	
	status = GLX_BAD_VISUAL;
    }

    /*
    ** If we can't find the config for this visual, this visual is not
    ** supported by the OpenGL implementation on the server.
    */
    if ( (status == GLX_BAD_VISUAL) && (attribute == GLX_USE_GL) ) {
	*value_return = GL_FALSE;
	status = Success;
    }

    return status;
}

/************************************************************************/

static void
init_fbconfig_for_chooser( __GLcontextModes * config,
			   GLboolean fbconfig_style_tags )
{
    memset( config, 0, sizeof( __GLcontextModes ) );
    config->visualID = (XID) GLX_DONT_CARE;
    config->visualType = GLX_DONT_CARE;

    /* glXChooseFBConfig specifies different defaults for these two than
     * glXChooseVisual.
     */
    if ( fbconfig_style_tags ) {
	config->rgbMode = GL_TRUE;
	config->doubleBufferMode = GLX_DONT_CARE;
    }

    config->visualRating = GLX_DONT_CARE;
    config->transparentPixel = GLX_NONE;
    config->transparentRed = GLX_DONT_CARE;
    config->transparentGreen = GLX_DONT_CARE;
    config->transparentBlue = GLX_DONT_CARE;
    config->transparentAlpha = GLX_DONT_CARE;
    config->transparentIndex = GLX_DONT_CARE;

    config->drawableType = GLX_WINDOW_BIT;
    config->renderType = (config->rgbMode) ? GLX_RGBA_BIT : GLX_COLOR_INDEX_BIT;
    config->xRenderable = GLX_DONT_CARE;
    config->fbconfigID = (GLXFBConfigID)(GLX_DONT_CARE);

    config->swapMethod = GLX_DONT_CARE;
}

#define MATCH_DONT_CARE( param ) \
	do { \
	    if ( (a-> param != GLX_DONT_CARE) \
		 && (a-> param != b-> param) ) { \
		return False; \
	    } \
	} while ( 0 )

#define MATCH_MINIMUM( param ) \
	do { \
	    if ( (a-> param != GLX_DONT_CARE) \
		 && (a-> param > b-> param) ) { \
		return False; \
	    } \
	} while ( 0 )

#define MATCH_EXACT( param ) \
	do { \
	    if ( a-> param != b-> param) { \
		return False; \
	    } \
	} while ( 0 )

/**
 * Determine if two GLXFBConfigs are compatible.
 *
 * \param a  Application specified config to test.
 * \param b  Server specified config to test against \c a.
 */
static Bool
fbconfigs_compatible( const __GLcontextModes * const a,
		      const __GLcontextModes * const b )
{
    MATCH_DONT_CARE( doubleBufferMode );
    MATCH_DONT_CARE( visualType );
    MATCH_DONT_CARE( visualRating );
    MATCH_DONT_CARE( xRenderable );
    MATCH_DONT_CARE( fbconfigID );
    MATCH_DONT_CARE( swapMethod );

    MATCH_MINIMUM( rgbBits );
    MATCH_MINIMUM( numAuxBuffers );
    MATCH_MINIMUM( redBits );
    MATCH_MINIMUM( greenBits );
    MATCH_MINIMUM( blueBits );
    MATCH_MINIMUM( alphaBits );
    MATCH_MINIMUM( depthBits );
    MATCH_MINIMUM( stencilBits );
    MATCH_MINIMUM( accumRedBits );
    MATCH_MINIMUM( accumGreenBits );
    MATCH_MINIMUM( accumBlueBits );
    MATCH_MINIMUM( accumAlphaBits );
    MATCH_MINIMUM( sampleBuffers );
    MATCH_MINIMUM( maxPbufferWidth );
    MATCH_MINIMUM( maxPbufferHeight );
    MATCH_MINIMUM( maxPbufferPixels );
    MATCH_MINIMUM( samples );

    MATCH_DONT_CARE( stereoMode );
    MATCH_EXACT( level );

    if ( ((a->drawableType & b->drawableType) == 0)
	 || ((a->renderType & b->renderType) == 0) ) {
	return False;
    }


    /* There is a bug in a few of the XFree86 DDX drivers.  They contain
     * visuals with a "transparent type" of 0 when they really mean GLX_NONE.
     * Technically speaking, it is a bug in the DDX driver, but there is
     * enough of an installed base to work around the problem here.  In any
     * case, 0 is not a valid value of the transparent type, so we'll treat 0 
     * from the app as GLX_DONT_CARE. We'll consider GLX_NONE from the app and
     * 0 from the server to be a match to maintain backward compatibility with
     * the (broken) drivers.
     */

    if ( a->transparentPixel != GLX_DONT_CARE
         && a->transparentPixel != 0 ) {
        if ( a->transparentPixel == GLX_NONE ) {
            if ( b->transparentPixel != GLX_NONE && b->transparentPixel != 0 )
                return False;
        } else {
            MATCH_EXACT( transparentPixel );
        }

	switch ( a->transparentPixel ) {
	  case GLX_TRANSPARENT_RGB:
	    MATCH_DONT_CARE( transparentRed );
	    MATCH_DONT_CARE( transparentGreen );
	    MATCH_DONT_CARE( transparentBlue );
	    MATCH_DONT_CARE( transparentAlpha );
	    break;

	  case GLX_TRANSPARENT_INDEX:
	    MATCH_DONT_CARE( transparentIndex );
	    break;

	  default:
	    break;
	}
    }

    return True;
}


/* There's some trickly language in the GLX spec about how this is supposed
 * to work.  Basically, if a given component size is either not specified
 * or the requested size is zero, it is supposed to act like PERFER_SMALLER.
 * Well, that's really hard to do with the code as-is.  This behavior is
 * closer to correct, but still not technically right.
 */
#define PREFER_LARGER_OR_ZERO(comp) \
    do { \
	if ( ((*a)-> comp) != ((*b)-> comp) ) { \
	    if ( ((*a)-> comp) == 0 ) { \
		return -1; \
	    } \
	    else if ( ((*b)-> comp) == 0 ) { \
		return 1; \
	    } \
	    else { \
		return ((*b)-> comp) - ((*a)-> comp) ; \
	    } \
	} \
    } while( 0 )

#define PREFER_LARGER(comp) \
    do { \
	if ( ((*a)-> comp) != ((*b)-> comp) ) { \
	    return ((*b)-> comp) - ((*a)-> comp) ; \
	} \
    } while( 0 )

#define PREFER_SMALLER(comp) \
    do { \
	if ( ((*a)-> comp) != ((*b)-> comp) ) { \
	    return ((*a)-> comp) - ((*b)-> comp) ; \
	} \
    } while( 0 )

/**
 * Compare two GLXFBConfigs.  This function is intended to be used as the
 * compare function passed in to qsort.
 * 
 * \returns If \c a is a "better" config, according to the specification of
 *          SGIX_fbconfig, a number less than zero is returned.  If \c b is
 *          better, then a number greater than zero is return.  If both are
 *          equal, zero is returned.
 * \sa qsort, glXChooseVisual, glXChooseFBConfig, glXChooseFBConfigSGIX
 */
static int
fbconfig_compare( const __GLcontextModes * const * const a,
		  const __GLcontextModes * const * const b )
{
    /* The order of these comparisons must NOT change.  It is defined by
     * the GLX 1.3 spec and ARB_multisample.
     */

    PREFER_SMALLER( visualSelectGroup );

    /* The sort order for the visualRating is GLX_NONE, GLX_SLOW, and
     * GLX_NON_CONFORMANT_CONFIG.  It just so happens that this is the
     * numerical sort order of the enums (0x8000, 0x8001, and 0x800D).
     */
    PREFER_SMALLER( visualRating );

    /* This isn't quite right.  It is supposed to compare the sum of the
     * components the user specifically set minimums for.
     */
    PREFER_LARGER_OR_ZERO( redBits );
    PREFER_LARGER_OR_ZERO( greenBits );
    PREFER_LARGER_OR_ZERO( blueBits );
    PREFER_LARGER_OR_ZERO( alphaBits );

    PREFER_SMALLER( rgbBits );

    if ( ((*a)->doubleBufferMode != (*b)->doubleBufferMode) ) {
	/* Prefer single-buffer.
	 */
	return ( !(*a)->doubleBufferMode ) ? -1 : 1;
    }

    PREFER_SMALLER( numAuxBuffers );

    PREFER_LARGER_OR_ZERO( depthBits );
    PREFER_SMALLER( stencilBits );

    /* This isn't quite right.  It is supposed to compare the sum of the
     * components the user specifically set minimums for.
     */
    PREFER_LARGER_OR_ZERO( accumRedBits );
    PREFER_LARGER_OR_ZERO( accumGreenBits );
    PREFER_LARGER_OR_ZERO( accumBlueBits );
    PREFER_LARGER_OR_ZERO( accumAlphaBits );

    PREFER_SMALLER( visualType );

    /* None of the multisample specs say where this comparison should happen,
     * so I put it near the end.
     */
    PREFER_SMALLER( sampleBuffers );
    PREFER_SMALLER( samples );

    /* None of the pbuffer or fbconfig specs say that this comparison needs
     * to happen at all, but it seems like it should.
     */
    PREFER_LARGER( maxPbufferWidth );
    PREFER_LARGER( maxPbufferHeight );
    PREFER_LARGER( maxPbufferPixels );

    return 0;
}


/**
 * Selects and sorts a subset of the supplied configs based on the attributes.
 * This function forms to basis of \c glXChooseVisual, \c glXChooseFBConfig,
 * and \c glXChooseFBConfigSGIX.
 * 
 * \param configs   Array of pointers to possible configs.  The elements of
 *                  this array that do not meet the criteria will be set to
 *                  NULL.  The remaining elements will be sorted according to
 *                  the various visual / FBConfig selection rules.
 * \param num_configs  Number of elements in the \c configs array.
 * \param attribList   Attributes used select from \c configs.  This array is
 *                     terminated by a \c None tag.  The array can either take
 *                     the form expected by \c glXChooseVisual (where boolean
 *                     tags do not have a value) or by \c glXChooseFBConfig
 *                     (where every tag has a value).
 * \param fbconfig_style_tags  Selects whether \c attribList is in
 *                             \c glXChooseVisual style or
 *                             \c glXChooseFBConfig style.
 * \returns The number of valid elements left in \c configs.
 * 
 * \sa glXChooseVisual, glXChooseFBConfig, glXChooseFBConfigSGIX
 */
static int
choose_visual( __GLcontextModes ** configs, int num_configs,
	       const int *attribList, GLboolean fbconfig_style_tags )
{
    __GLcontextModes    test_config;
    int   base;
    int   i;

    /* This is a fairly direct implementation of the selection method
     * described by GLX_SGIX_fbconfig.  Start by culling out all the
     * configs that are not compatible with the selected parameter
     * list.
     */

    init_fbconfig_for_chooser( & test_config, fbconfig_style_tags );
    __glXInitializeVisualConfigFromTags( & test_config, 512,
					 (const INT32 *) attribList,
					 GL_TRUE, fbconfig_style_tags );

    base = 0;
    for ( i = 0 ; i < num_configs ; i++ ) {
	if ( fbconfigs_compatible( & test_config, configs[i] ) ) {
	    configs[ base ] = configs[ i ];
	    base++;
	}
    }

    if ( base == 0 ) {
	return 0;
    }
 
    if ( base < num_configs ) {
	(void) memset( & configs[ base ], 0, 
		       sizeof( void * ) * (num_configs - base) );
    }

    /* After the incompatible configs are removed, the resulting
     * list is sorted according to the rules set out in the various
     * specifications.
     */
    
    qsort( configs, base, sizeof( __GLcontextModes * ),
	   (int (*)(const void*, const void*)) fbconfig_compare );
    return base;
}




/*
** Return the visual that best matches the template.  Return None if no
** visual matches the template.
*/
PUBLIC XVisualInfo *glXChooseVisual(Display *dpy, int screen, int *attribList)
{
    XVisualInfo *visualList = NULL;
    __GLXdisplayPrivate *priv;
    __GLXscreenConfigs *psc;
    __GLcontextModes  test_config;
    __GLcontextModes *modes;
    const __GLcontextModes *best_config = NULL;

    /*
    ** Get a list of all visuals, return if list is empty
    */
    if ( GetGLXPrivScreenConfig( dpy, screen, & priv, & psc ) != Success ) {
	return None;
    }
   

    /*
    ** Build a template from the defaults and the attribute list
    ** Free visual list and return if an unexpected token is encountered
    */
    init_fbconfig_for_chooser( & test_config, GL_FALSE );
    __glXInitializeVisualConfigFromTags( & test_config, 512, 
					 (const INT32 *) attribList,
					 GL_TRUE, GL_FALSE );

    /*
    ** Eliminate visuals that don't meet minimum requirements
    ** Compute a score for those that do
    ** Remember which visual, if any, got the highest score
    */
    for ( modes = psc->visuals ; modes != NULL ; modes = modes->next ) {
	if ( fbconfigs_compatible( & test_config, modes )
	     && ((best_config == NULL)
		 || (fbconfig_compare( (const __GLcontextModes * const * const)&modes, &best_config ) < 0)) ) {
	    best_config = modes;
	}
    }

    /*
    ** If no visual is acceptable, return None
    ** Otherwise, create an XVisualInfo list with just the selected X visual
    ** and return this.
    */
    if (best_config != NULL) {
	XVisualInfo visualTemplate;
	int  i;

	visualTemplate.screen = screen;
	visualTemplate.visualid = best_config->visualID;
	visualList = XGetVisualInfo( dpy, VisualScreenMask|VisualIDMask,
				     &visualTemplate, &i );
    }

    if(visualList && getenv("LIBGL_DUMP_VISUALID")) {
	printf("visualid 0x%lx\n", visualList[0].visualid);
    }

    return visualList;
}


PUBLIC const char *glXQueryExtensionsString( Display *dpy, int screen )
{
    __GLXscreenConfigs *psc;
    __GLXdisplayPrivate *priv;

    if ( GetGLXPrivScreenConfig( dpy, screen, & priv, & psc ) != Success ) {
	return NULL;
    }

    if (!psc->effectiveGLXexts) {
        if (!psc->serverGLXexts) {
	    psc->serverGLXexts = __glXGetStringFromServer(dpy, priv->majorOpcode,
					  	   X_GLXQueryServerString,
					  	   screen, GLX_EXTENSIONS);
	}

	__glXCalculateUsableExtensions(psc,
#ifdef GLX_DIRECT_RENDERING
				       (psc->driScreen != NULL),
#else
				       GL_FALSE,
#endif
				       priv->minorVersion);
    }

    return psc->effectiveGLXexts;
}

PUBLIC const char *glXGetClientString( Display *dpy, int name )
{
    switch(name) {
	case GLX_VENDOR:
	    return (__glXGLXClientVendorName);
	case GLX_VERSION:
	    return (__glXGLXClientVersion);
	case GLX_EXTENSIONS:
	    return (__glXGetClientExtensions());
	default:
	    return NULL;
    }
}

PUBLIC const char *glXQueryServerString( Display *dpy, int screen, int name )
{
    __GLXscreenConfigs *psc;
    __GLXdisplayPrivate *priv;
    const char ** str;


    if ( GetGLXPrivScreenConfig( dpy, screen, & priv, & psc ) != Success ) {
	return NULL;
    }

    switch(name) {
	case GLX_VENDOR:
	    str = & priv->serverGLXvendor;
	    break;
	case GLX_VERSION:
	    str = & priv->serverGLXversion;
	    break;
	case GLX_EXTENSIONS:
	    str = & psc->serverGLXexts;
	    break;
	default:
	    return NULL;
    }

    if ( *str == NULL ) {
	*str = __glXGetStringFromServer(dpy, priv->majorOpcode,
					X_GLXQueryServerString, screen, name);
    }
    
    return *str;
}

void __glXClientInfo (  Display *dpy, int opcode  )
{
    xGLXClientInfoReq *req;
    int size;
    char * ext_str = __glXGetClientGLExtensionString();

    /* Send the glXClientInfo request */
    LockDisplay(dpy);
    GetReq(GLXClientInfo,req);
    req->reqType = opcode;
    req->glxCode = X_GLXClientInfo;
    req->major = GLX_MAJOR_VERSION;
    req->minor = GLX_MINOR_VERSION;

    size = strlen( ext_str ) + 1;
    req->length += (size + 3) >> 2;
    req->numbytes = size;
    Data(dpy, ext_str, size);

    UnlockDisplay(dpy);
    SyncHandle();
    
    Xfree( ext_str );
}


/*
** EXT_import_context
*/

PUBLIC Display *glXGetCurrentDisplay(void)
{
    GLXContext gc = __glXGetCurrentContext();
    if (NULL == gc) return NULL;
    return gc->currentDpy;
}

PUBLIC GLX_ALIAS(Display *, glXGetCurrentDisplayEXT, (void), (),
	  glXGetCurrentDisplay)

#if 0
/**
 * Used internally by libGL to send \c xGLXQueryContextinfoExtReq requests
 * to the X-server.
 * 
 * \param dpy  Display where \c ctx was created.
 * \param ctx  Context to query.
 * \returns  \c Success on success.  \c GLX_BAD_CONTEXT if \c ctx is invalid,
 *           or zero if the request failed due to internal problems (i.e.,
 *           unable to allocate temporary memory, etc.)
 * 
 * \note
 * This function dynamically determines whether to use the EXT_import_context
 * version of the protocol or the GLX 1.3 version of the protocol.
 */
static int __glXQueryContextInfo(Display *dpy, GLXContext ctx)
{
    __GLXdisplayPrivate *priv = __glXInitialize(dpy);
    xGLXQueryContextReply reply;
    CARD8 opcode;
    GLuint numValues;
    int retval;

    if (ctx == NULL) {
	return GLX_BAD_CONTEXT;
    }
    opcode = __glXSetupForCommand(dpy);
    if (!opcode) {
	return 0;
    }

    /* Send the glXQueryContextInfoEXT request */
    LockDisplay(dpy);

    if ( (priv->majorVersion > 1) || (priv->minorVersion >= 3) ) {
	xGLXQueryContextReq *req;

	GetReq(GLXQueryContext, req);

	req->reqType = opcode;
	req->glxCode = X_GLXQueryContext;
	req->context = (unsigned int)(ctx->xid);
    }
    else {
	xGLXVendorPrivateReq *vpreq;
	xGLXQueryContextInfoEXTReq *req;

	GetReqExtra( GLXVendorPrivate,
		     sz_xGLXQueryContextInfoEXTReq - sz_xGLXVendorPrivateReq,
		     vpreq );
	req = (xGLXQueryContextInfoEXTReq *)vpreq;
	req->reqType = opcode;
	req->glxCode = X_GLXVendorPrivateWithReply;
	req->vendorCode = X_GLXvop_QueryContextInfoEXT;
	req->context = (unsigned int)(ctx->xid);
    }

    _XReply(dpy, (xReply*) &reply, 0, False);

    numValues = reply.n;
    if (numValues == 0)
	retval = Success;
    else if (numValues > __GLX_MAX_CONTEXT_PROPS)
	retval = 0;
    else
    {
	int *propList, *pProp;
	int nPropListBytes;
	int i;

	nPropListBytes = numValues << 3;
	propList = (int *) Xmalloc(nPropListBytes);
	if (NULL == propList) {
	    retval = 0;
	} else {
	    _XRead(dpy, (char *)propList, nPropListBytes);
	    pProp = propList;
	    for (i=0; i < numValues; i++) {
		switch (*pProp++) {
		case GLX_SHARE_CONTEXT_EXT:
		    ctx->share_xid = *pProp++;
		    break;
		case GLX_VISUAL_ID_EXT:
		    ctx->mode =
			_gl_context_modes_find_visual(ctx->psc->visuals, *pProp++);
		    break;
		case GLX_SCREEN:
		    ctx->screen = *pProp++;
		    break;
		case GLX_FBCONFIG_ID:
		    ctx->mode =
			_gl_context_modes_find_fbconfig(ctx->psc->configs, *pProp++);
		    break;
		case GLX_RENDER_TYPE:
		    ctx->renderType = *pProp++;
		    break;
		default:
		    pProp++;
		    continue;
		}
	    }
	    Xfree((char *)propList);
	    retval = Success;
	}
    }
    UnlockDisplay(dpy);
    SyncHandle();
    return retval;
}
#endif

PUBLIC int
glXQueryContext(Display *dpy, GLXContext ctx, int attribute, int *value)
{
    int retVal;

    (void)retVal;

#if 0 /*AppleSGLX*/
    /* get the information from the server if we don't have it already */
#ifdef GLX_DIRECT_RENDERING
    if (!ctx->driContext && (ctx->mode == NULL)) {
#else
    if (ctx->mode == NULL) {
#endif
	retVal = __glXQueryContextInfo(dpy, ctx);
	if (Success != retVal) return retVal;
    }
#endif

    switch (attribute) {
#if 0
    case GLX_SHARE_CONTEXT_EXT:
	*value = (int)(ctx->share_xid);
	break;
    case GLX_VISUAL_ID_EXT:
	*value = ctx->mode ? ctx->mode->visualID : None;
	break;
#endif
    case GLX_SCREEN:
	*value = (int)(ctx->screen);
	break;
    case GLX_FBCONFIG_ID:
	*value = ctx->mode ? ctx->mode->fbconfigID : None;
	break;
    case GLX_RENDER_TYPE:
	*value = (int)(ctx->renderType);
	break;
    default:
	return GLX_BAD_ATTRIBUTE;
    }
    return Success;
}

PUBLIC GLX_ALIAS( int, glXQueryContextInfoEXT,
	   (Display *dpy, GLXContext ctx, int attribute, int *value),
	   (dpy, ctx, attribute, value),
	   glXQueryContext )

PUBLIC GLXContextID glXGetContextIDEXT(const GLXContext ctx)
{
    return ctx->xid;
}

PUBLIC GLXContext glXImportContextEXT(Display *dpy, GLXContextID contextID)
{
#if 0
    GLXContext ctx;

    if (contextID == None) {
	return NULL;
    }
    if (__glXIsDirect(dpy, contextID)) {
	return NULL;
    }

    ctx = CreateContext(dpy, NULL, NULL, NULL, False, contextID, False, 0);
    if (NULL != ctx) {
	if (Success != __glXQueryContextInfo(dpy, ctx)) {
	   return NULL;
	}
    }
    return ctx;
#endif
    return NULL;
}

PUBLIC void glXFreeContextEXT(Display *dpy, GLXContext ctx)
{
    DestroyContext(dpy, ctx);
}



/*
 * GLX 1.3 functions - these are just stubs for now!
 */

PUBLIC GLXFBConfig *glXChooseFBConfig(Display *dpy, int screen,
				      const int *attribList, int *nitems)
{
    __GLcontextModes ** config_list;
    int   list_size;


    config_list = (__GLcontextModes **) 
	glXGetFBConfigs( dpy, screen, & list_size );

    if ( (config_list != NULL) && (list_size > 0) && (attribList != NULL) ) {
	list_size = choose_visual( config_list, list_size, attribList,
				   GL_TRUE );
	if ( list_size == 0 ) {
	    XFree( config_list );
	    config_list = NULL;
	}
    }

    *nitems = list_size;
    return (GLXFBConfig *) config_list;
}


PUBLIC GLXContext glXCreateNewContext(Display *dpy, GLXFBConfig config,
				      int renderType, GLXContext shareList,
				      Bool allowDirect)
{
    return CreateContext( dpy, NULL, (__GLcontextModes *) config, shareList,
			  allowDirect, None, True, renderType );
}


PUBLIC GLXDrawable glXGetCurrentReadDrawable(void)
{
    GLXContext gc = __glXGetCurrentContext();
    return gc->currentReadable;
}


PUBLIC GLXFBConfig *glXGetFBConfigs(Display *dpy, int screen, int *nelements)
{
    __GLXdisplayPrivate *priv = __glXInitialize(dpy);
    __GLcontextModes ** config = NULL;
    int   i;

    *nelements = 0;
    if ( (priv->screenConfigs != NULL)
	 && (screen >= 0) && (screen <= ScreenCount(dpy))
	 && (priv->screenConfigs[screen].configs != NULL)
	 && (priv->screenConfigs[screen].configs->fbconfigID != GLX_DONT_CARE) ) {
	unsigned num_configs = 0;
	__GLcontextModes * modes;


	for ( modes = priv->screenConfigs[screen].configs
	      ; modes != NULL
	      ; modes = modes->next ) {
	    if ( modes->fbconfigID != GLX_DONT_CARE ) {
		num_configs++;
	    }
	}

	config = (__GLcontextModes **) Xmalloc( sizeof(__GLcontextModes *)
						* num_configs );
	if ( config != NULL ) {
	    *nelements = num_configs;
	    i = 0;
	    for ( modes = priv->screenConfigs[screen].configs
		  ; modes != NULL
		  ; modes = modes->next ) {
		if ( modes->fbconfigID != GLX_DONT_CARE ) {
		    config[i] = modes;
		    i++;
		}
	    }
	}
    }
    return (GLXFBConfig *) config;
}


PUBLIC int glXGetFBConfigAttrib(Display *dpy, GLXFBConfig config,
				int attribute, int *value)
{
    __GLcontextModes * const modes = ValidateGLXFBConfig( dpy, config );

    return (modes != NULL)
	? _gl_get_context_mode_data( modes, attribute, value )
	: GLXBadFBConfig;
}


PUBLIC XVisualInfo *glXGetVisualFromFBConfig(Display *dpy, GLXFBConfig config)
{
    XVisualInfo visualTemplate;
    __GLcontextModes * fbconfig = (__GLcontextModes *) config;
    int  count;

    /*
    ** Get a list of all visuals, return if list is empty
    */
    visualTemplate.visualid = fbconfig->visualID;
    return XGetVisualInfo(dpy,VisualIDMask,&visualTemplate,&count);
}

/*
** GLX_SGIX_fbconfig
** Many of these functions are aliased to GLX 1.3 entry points in the 
** GLX_functions table.
*/

PUBLIC GLX_ALIAS(int, glXGetFBConfigAttribSGIX,
	  (Display *dpy, GLXFBConfigSGIX config, int attribute, int *value),
	  (dpy, config, attribute, value),
	  glXGetFBConfigAttrib)

PUBLIC GLX_ALIAS(GLXFBConfigSGIX *, glXChooseFBConfigSGIX,
	  (Display *dpy, int screen, int *attrib_list, int *nelements),
	  (dpy, screen, attrib_list, nelements),
	  glXChooseFBConfig)

PUBLIC GLX_ALIAS(XVisualInfo *, glXGetVisualFromFBConfigSGIX,
	  (Display * dpy, GLXFBConfigSGIX config),
	  (dpy, config),
	  glXGetVisualFromFBConfig)

PUBLIC GLXPixmap glXCreateGLXPixmapWithConfigSGIX(Display *dpy,
                            GLXFBConfigSGIX config, Pixmap pixmap)
{
    xGLXVendorPrivateWithReplyReq *vpreq;
    xGLXCreateGLXPixmapWithConfigSGIXReq *req;
    GLXPixmap xid = None;
    CARD8 opcode;
    const __GLcontextModes * const fbconfig = (__GLcontextModes *) config;
    __GLXscreenConfigs * psc;


    if ( (dpy == NULL) || (config == NULL) ) {
	return None;
    }

    if(apple_glx_pixmap_create(dpy, fbconfig->screen, pixmap, fbconfig))
	return None;

    return pixmap;


    psc = GetGLXScreenConfigs( dpy, fbconfig->screen );
    if ( (psc != NULL) 
	 && __glXExtensionBitIsEnabled( psc, SGIX_fbconfig_bit ) ) {
	opcode = __glXSetupForCommand(dpy);
	if (!opcode) {
	    return None;
	}

	/* Send the glXCreateGLXPixmapWithConfigSGIX request */
	LockDisplay(dpy);
	GetReqExtra(GLXVendorPrivateWithReply,
		    sz_xGLXCreateGLXPixmapWithConfigSGIXReq-sz_xGLXVendorPrivateWithReplyReq,vpreq);
	req = (xGLXCreateGLXPixmapWithConfigSGIXReq *)vpreq;
	req->reqType = opcode;
	req->glxCode = X_GLXVendorPrivateWithReply;
	req->vendorCode = X_GLXvop_CreateGLXPixmapWithConfigSGIX;
	req->screen = fbconfig->screen;
	req->fbconfig = fbconfig->fbconfigID;
	req->pixmap = pixmap;
	req->glxpixmap = xid = XAllocID(dpy);
	UnlockDisplay(dpy);
	SyncHandle();
    }

    return xid;
}

PUBLIC GLXContext glXCreateContextWithConfigSGIX(Display *dpy,
                             GLXFBConfigSGIX config, int renderType,
                             GLXContext shareList, Bool allowDirect)
{
    GLXContext gc = NULL;
    const __GLcontextModes * const fbconfig = (__GLcontextModes *) config;
    __GLXscreenConfigs * psc;


    if ( (dpy == NULL) || (config == NULL) ) {
	return None;
    }

    psc = GetGLXScreenConfigs( dpy, fbconfig->screen );
    if ( (psc != NULL) 
	 && __glXExtensionBitIsEnabled( psc, SGIX_fbconfig_bit ) ) {
	gc = CreateContext( dpy, NULL, (__GLcontextModes *) config, shareList,
			    allowDirect, None, False, renderType );
    }

    return gc;
}


PUBLIC GLXFBConfigSGIX glXGetFBConfigFromVisualSGIX(Display *dpy,
						    XVisualInfo *vis)
{
    __GLXdisplayPrivate *priv;
    __GLXscreenConfigs *psc;

    if ( (GetGLXPrivScreenConfig( dpy, vis->screen, & priv, & psc ) != Success)
	 && __glXExtensionBitIsEnabled( psc, SGIX_fbconfig_bit )
	 && (psc->configs->fbconfigID != GLX_DONT_CARE) ) {
	return (GLXFBConfigSGIX) _gl_context_modes_find_visual( psc->configs,
								vis->visualid );
    }

    return NULL;
}

#if 0 /* AppleSGLX may someday need to support this for X11 OpenGL
	 compositing managers. */
/**
 * GLX_EXT_texture_from_pixmap
 */
/*@{*/
static void __glXBindTexImageEXT(Display *dpy,
				 GLXDrawable drawable,
				 int buffer,
				 const int *attrib_list)
{
    xGLXVendorPrivateReq *req;
    GLXContext gc = __glXGetCurrentContext();
    CARD32 *drawable_ptr;
    INT32 *buffer_ptr;
    CARD32 *num_attrib_ptr;
    CARD32 *attrib_ptr;
    CARD8 opcode;
    unsigned int i;

    if (gc == NULL)
	return;

    i = 0;
    if (attrib_list) {
 	while (attrib_list[i * 2] != None)
 	    i++;
    }
 
#ifdef GLX_DIRECT_RENDERING
    if (gc->driContext) {
	__GLXDRIdrawable *pdraw = GetGLXDRIDrawable(dpy, drawable, NULL);

	if (pdraw != NULL)
	    (*pdraw->psc->texBuffer->setTexBuffer)(gc->__driContext,
						   pdraw->textureTarget,
						   pdraw->driDrawable);

	return;
    }
#endif

    opcode = __glXSetupForCommand(dpy);
    if (!opcode)
	return;

    LockDisplay(dpy);
    GetReqExtra(GLXVendorPrivate, 12 + 8 * i,req);
    req->reqType = opcode;
    req->glxCode = X_GLXVendorPrivate;
    req->vendorCode = X_GLXvop_BindTexImageEXT;
    req->contextTag = gc->currentContextTag;

    drawable_ptr = (CARD32 *) (req + 1);
    buffer_ptr = (INT32 *) (drawable_ptr + 1);
    num_attrib_ptr = (CARD32 *) (buffer_ptr + 1);
    attrib_ptr = (CARD32 *) (num_attrib_ptr + 1);

    *drawable_ptr = drawable;
    *buffer_ptr = buffer;
    *num_attrib_ptr = (CARD32) i;

    i = 0;
    if (attrib_list) {
 	while (attrib_list[i * 2] != None)
 	{
 	    *attrib_ptr++ = (CARD32) attrib_list[i * 2 + 0];
 	    *attrib_ptr++ = (CARD32) attrib_list[i * 2 + 1];
 	    i++;
 	}
    }

    UnlockDisplay(dpy);
    SyncHandle();
}

static void __glXReleaseTexImageEXT(Display *dpy,
				    GLXDrawable drawable,
				    int buffer)
{
    xGLXVendorPrivateReq *req;
    GLXContext gc = __glXGetCurrentContext();
    CARD32 *drawable_ptr;
    INT32 *buffer_ptr;
    CARD8 opcode;

    if (gc == NULL)
	return;

#ifdef GLX_DIRECT_RENDERING
    if (gc->driContext)
	return;
#endif

    opcode = __glXSetupForCommand(dpy);
    if (!opcode)
	return;

    LockDisplay(dpy);
    GetReqExtra(GLXVendorPrivate, sizeof(CARD32)+sizeof(INT32),req);
    req->reqType = opcode;
    req->glxCode = X_GLXVendorPrivate;
    req->vendorCode = X_GLXvop_ReleaseTexImageEXT;
    req->contextTag = gc->currentContextTag;

    drawable_ptr = (CARD32 *) (req + 1);
    buffer_ptr = (INT32 *) (drawable_ptr + 1);

    *drawable_ptr = drawable;
    *buffer_ptr = buffer;

    UnlockDisplay(dpy);
    SyncHandle();
}
/*@}*/

#endif

/**
 * \c strdup is actually not a standard ANSI C or POSIX routine.
 * Irix will not define it if ANSI mode is in effect.
 * 
 * \sa strdup
 */
_X_HIDDEN char *
__glXstrdup(const char *str)
{
   char *copy;
   copy = (char *) Xmalloc(strlen(str) + 1);
   if (!copy)
      return NULL;
   strcpy(copy, str);
   return copy;
}

/*
** glXGetProcAddress support
*/

struct name_address_pair {
   const char *Name;
   GLvoid *Address;
};

#define GLX_FUNCTION(f) { # f, (GLvoid *) f }
#define GLX_FUNCTION2(n,f) { # n, (GLvoid *) f }

static const struct name_address_pair GLX_functions[] = {
   /*** GLX_VERSION_1_0 ***/
   GLX_FUNCTION( glXChooseVisual ),
   GLX_FUNCTION( glXCopyContext ),
   GLX_FUNCTION( glXCreateContext ),
   GLX_FUNCTION( glXCreateGLXPixmap ),
   GLX_FUNCTION( glXDestroyContext ),
   GLX_FUNCTION( glXDestroyGLXPixmap ),
   GLX_FUNCTION( glXGetConfig ),
   GLX_FUNCTION( glXGetCurrentContext ),
   GLX_FUNCTION( glXGetCurrentDrawable ),
   GLX_FUNCTION( glXIsDirect ),
   GLX_FUNCTION( glXMakeCurrent ),
   GLX_FUNCTION( glXQueryExtension ),
   GLX_FUNCTION( glXQueryVersion ),
   GLX_FUNCTION( glXSwapBuffers ),
   GLX_FUNCTION( glXUseXFont ),
   GLX_FUNCTION( glXWaitGL ),
   GLX_FUNCTION( glXWaitX ),

   /*** GLX_VERSION_1_1 ***/
   GLX_FUNCTION( glXGetClientString ),
   GLX_FUNCTION( glXQueryExtensionsString ),
   GLX_FUNCTION( glXQueryServerString ),

   /*** GLX_VERSION_1_2 ***/
   GLX_FUNCTION( glXGetCurrentDisplay ),

   /*** GLX_VERSION_1_3 ***/
   GLX_FUNCTION( glXChooseFBConfig ),
   GLX_FUNCTION( glXCreateNewContext ),
   GLX_FUNCTION( glXCreatePbuffer ),
   GLX_FUNCTION( glXCreatePixmap ),
   GLX_FUNCTION( glXCreateWindow ),
   GLX_FUNCTION( glXDestroyPbuffer ),
   GLX_FUNCTION( glXDestroyPixmap ),
   GLX_FUNCTION( glXDestroyWindow ),
   GLX_FUNCTION( glXGetCurrentReadDrawable ),
   GLX_FUNCTION( glXGetFBConfigAttrib ),
   GLX_FUNCTION( glXGetFBConfigs ),
   GLX_FUNCTION( glXGetSelectedEvent ),
   GLX_FUNCTION( glXGetVisualFromFBConfig ),
   GLX_FUNCTION( glXMakeContextCurrent ),
   GLX_FUNCTION( glXQueryContext ),
   GLX_FUNCTION( glXQueryDrawable ),
   GLX_FUNCTION( glXSelectEvent ),

#if 0
   /*** GLX_SGI_swap_control ***/
   GLX_FUNCTION2( glXSwapIntervalSGI, __glXSwapIntervalSGI ),

   /*** GLX_SGI_video_sync ***/
   GLX_FUNCTION2( glXGetVideoSyncSGI, __glXGetVideoSyncSGI ),
   GLX_FUNCTION2( glXWaitVideoSyncSGI, __glXWaitVideoSyncSGI ),

   /*** GLX_SGI_make_current_read ***/
   GLX_FUNCTION2( glXMakeCurrentReadSGI, glXMakeContextCurrent ),
   GLX_FUNCTION2( glXGetCurrentReadDrawableSGI, glXGetCurrentReadDrawable ),

   /*** GLX_EXT_import_context ***/
   GLX_FUNCTION( glXFreeContextEXT ),
   GLX_FUNCTION( glXGetContextIDEXT ),
   GLX_FUNCTION2( glXGetCurrentDisplayEXT, glXGetCurrentDisplay ),
   GLX_FUNCTION( glXImportContextEXT ),
   GLX_FUNCTION2( glXQueryContextInfoEXT, glXQueryContext ),
#endif

   /*** GLX_SGIX_fbconfig ***/
   GLX_FUNCTION2( glXGetFBConfigAttribSGIX, glXGetFBConfigAttrib ),
   GLX_FUNCTION2( glXChooseFBConfigSGIX, glXChooseFBConfig ),
   GLX_FUNCTION( glXCreateGLXPixmapWithConfigSGIX ),
   GLX_FUNCTION( glXCreateContextWithConfigSGIX ),
   GLX_FUNCTION2( glXGetVisualFromFBConfigSGIX, glXGetVisualFromFBConfig ),
   GLX_FUNCTION( glXGetFBConfigFromVisualSGIX ),

#if 0
   /*** GLX_SGIX_pbuffer ***/
   GLX_FUNCTION( glXCreateGLXPbufferSGIX ),
   GLX_FUNCTION( glXDestroyGLXPbufferSGIX ),
   GLX_FUNCTION( glXQueryGLXPbufferSGIX ),
   GLX_FUNCTION( glXSelectEventSGIX ),
   GLX_FUNCTION( glXGetSelectedEventSGIX ),

   /*** GLX_SGIX_swap_group ***/
   GLX_FUNCTION2( glXJoinSwapGroupSGIX, __glXJoinSwapGroupSGIX ),

   /*** GLX_SGIX_swap_barrier ***/
   GLX_FUNCTION2( glXBindSwapBarrierSGIX, __glXBindSwapBarrierSGIX ),
   GLX_FUNCTION2( glXQueryMaxSwapBarriersSGIX, __glXQueryMaxSwapBarriersSGIX ),

   /*** GLX_MESA_allocate_memory ***/
   GLX_FUNCTION( glXAllocateMemoryMESA ),
   GLX_FUNCTION( glXFreeMemoryMESA ),
   GLX_FUNCTION( glXGetMemoryOffsetMESA ),

   /*** GLX_MESA_copy_sub_buffer ***/
   GLX_FUNCTION2( glXCopySubBufferMESA, __glXCopySubBufferMESA ),

   /*** GLX_MESA_pixmap_colormap ***/
   GLX_FUNCTION( glXCreateGLXPixmapMESA ),

   /*** GLX_MESA_release_buffers ***/
   GLX_FUNCTION2( glXReleaseBuffersMESA, __glXReleaseBuffersMESA ),

   /*** GLX_MESA_swap_control ***/
   GLX_FUNCTION2( glXSwapIntervalMESA, __glXSwapIntervalMESA ),
   GLX_FUNCTION2( glXGetSwapIntervalMESA, __glXGetSwapIntervalMESA ),

   /*** GLX_MESA_swap_frame_usage ***/
   GLX_FUNCTION2( glXBeginFrameTrackingMESA, __glXBeginFrameTrackingMESA ),
   GLX_FUNCTION2( glXEndFrameTrackingMESA, __glXEndFrameTrackingMESA ),
   GLX_FUNCTION2( glXGetFrameUsageMESA, __glXGetFrameUsageMESA ),
   GLX_FUNCTION2( glXQueryFrameTrackingMESA, __glXQueryFrameTrackingMESA ),
#endif

   /*** GLX_ARB_get_proc_address ***/
   GLX_FUNCTION( glXGetProcAddressARB ),

   /*** GLX 1.4 ***/
   GLX_FUNCTION2( glXGetProcAddress, glXGetProcAddressARB ),

#if 0
   /*** GLX_OML_sync_control ***/
   GLX_FUNCTION2( glXWaitForSbcOML, __glXWaitForSbcOML ),
   GLX_FUNCTION2( glXWaitForMscOML, __glXWaitForMscOML ),
   GLX_FUNCTION2( glXSwapBuffersMscOML, __glXSwapBuffersMscOML ),
   GLX_FUNCTION2( glXGetMscRateOML, __glXGetMscRateOML ),
   GLX_FUNCTION2( glXGetSyncValuesOML, __glXGetSyncValuesOML ),

   /*** GLX_EXT_texture_from_pixmap ***/
   GLX_FUNCTION2( glXBindTexImageEXT, __glXBindTexImageEXT ),
   GLX_FUNCTION2( glXReleaseTexImageEXT, __glXReleaseTexImageEXT ),
#endif

#ifdef GLX_DIRECT_RENDERING
   /*** DRI configuration ***/
   GLX_FUNCTION( glXGetScreenDriver ),
   GLX_FUNCTION( glXGetDriverConfig ),
#endif

   { NULL, NULL }   /* end of list */
};


/**
 * Get the address of a named GL function.  This is the pre-GLX 1.4 name for
 * \c glXGetProcAddress.
 *
 * \param procName  Name of a GL or GLX function.
 * \returns         A pointer to the named function
 *
 * \sa glXGetProcAddress
 */
PUBLIC void (*glXGetProcAddressARB(const GLubyte *procName))( void )
{
   typedef void (*gl_function)( void );
   gl_function f;

   /* Search the table of GLX and internal functions first.  If that
    * fails and the supplied name could be a valid core GL name, try
    * searching the core GL function table.  This check is done to prevent
    * DRI based drivers from searching the core GL function table for
    * internal API functions.
    */
   
   f = (gl_function) apple_glx_get_proc_address(procName);

   return f;
}

/**
 * Get the address of a named GL function.  This is the GLX 1.4 name for
 * \c glXGetProcAddressARB.
 *
 * \param procName  Name of a GL or GLX function.
 * \returns         A pointer to the named function
 *
 * \sa glXGetProcAddressARB
 */
PUBLIC void (*glXGetProcAddress(const GLubyte *procName))( void )
#if defined(__GNUC__) && !defined(GLX_ALIAS_UNSUPPORTED)
    __attribute__ ((alias ("glXGetProcAddressARB")));
#else
{
   return glXGetProcAddressARB(procName);
}
#endif /* __GNUC__ */


#ifdef GLX_DIRECT_RENDERING
/**
 * Get the unadjusted system time (UST).  Currently, the UST is measured in
 * microseconds since Epoc.  The actual resolution of the UST may vary from
 * system to system, and the units may vary from release to release.
 * Drivers should not call this function directly.  They should instead use
 * \c glXGetProcAddress to obtain a pointer to the function.
 *
 * \param ust Location to store the 64-bit UST
 * \returns Zero on success or a negative errno value on failure.
 * 
 * \sa glXGetProcAddress, PFNGLXGETUSTPROC
 *
 * \since Internal API version 20030317.
 */
_X_HIDDEN int __glXGetUST( int64_t * ust )
{
    struct timeval  tv;
    
    if ( ust == NULL ) {
	return -EFAULT;
    }

    if ( gettimeofday( & tv, NULL ) == 0 ) {
	ust[0] = (tv.tv_sec * 1000000) + tv.tv_usec;
	return 0;
    } else {
	return -errno;
    }
}
#endif /* GLX_DIRECT_RENDERING */
