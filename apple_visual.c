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
#include <GL/gl.h>
#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLContext.h>
#include "glcontextmodes.h" 
#include "apple_cgl.h"

/*mode is a __GlcontextModes*/
void apple_visual_create_pfobj(CGLPixelFormatObj *pfobj, const void *mode,
			       bool *double_buffered) {
    CGLPixelFormatAttribute attr[60];
    const __GLcontextModes *c = mode;
    int numattr = 0;
    GLint vsref = 0;
    CGLError error = 0;

#if 0
    /*TODO make this be based on the preferences.*/
    attr[numattr++] = kCGLPFAOffScreen;
    
    attr[numattr++] = kCGLPFAColorSize;
    attr[numattr++] = 32;
#else

    if(c->stereoMode) 
	attr[numattr++] = kCGLPFAStereo;
    
    if(c->doubleBufferMode) {
	attr[numattr++] = kCGLPFADoubleBuffer;
	*double_buffered = true;
    } else {
	*double_buffered = false;
    }

    attr[numattr++] = kCGLPFAColorSize;
    attr[numattr++] = c->redBits + c->greenBits + c->blueBits;
    attr[numattr++] = kCGLPFAAlphaSize;
    attr[numattr++] = c->alphaBits;

    if((c->accumRedBits + c->accumGreenBits + c->accumBlueBits) > 0) {
	attr[numattr++] = kCGLPFAAccumSize;
	attr[numattr++] = c->accumRedBits + c->accumGreenBits + 
	    c->accumBlueBits + c->accumAlphaBits;
    }

    if(c->depthBits > 0) {
	attr[numattr++] = kCGLPFADepthSize;
	attr[numattr++] = c->depthBits;
    }

    if(c->stencilBits > 0) {
	attr[numattr++] = kCGLPFAStencilSize;
	attr[numattr++] = c->stencilBits;
    }

    if(c->sampleBuffers > 0) {
	attr[numattr++] = kCGLPFASampleBuffers;
        attr[numattr++] = c->sampleBuffers;
	attr[numattr++] = kCGLPFASamples;
	attr[numattr++] = c->samples;
    }
#endif
    
    attr[numattr++] = 0;

    error = apple_cgl.choose_pixel_format(attr, pfobj, &vsref);

    if(error) {
	fprintf(stderr, "error: %s\n", apple_cgl.error_string(error));
	abort();
    }
 }
