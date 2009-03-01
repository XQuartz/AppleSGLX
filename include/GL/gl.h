/*
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
 * Copyright (C) 2009 Apple Inc.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __X_GL_H
#define __X_GL_H

#ifdef GL_GLEXT_LEGACY 
/* The user has requested the exclusion of glext.h. */
#include "/System/Library/Frameworks/OpenGL.framework/Headers/gl.h"
#else
#define GL_GLEXT_LEGACY 1
#include "/System/Library/Frameworks/OpenGL.framework/Headers/gl.h"
#undef GL_GLEXT_LEGACY
/* Our glext.h is based on a version from the registry that is newer. */
#include <GL/glext.h>
#endif

/* 
 * This is needed for building apple_glx_pbuffer.c, the latest
 * glext.h from the registry lacks it, so it's from the Leopard glext.h: 
 */
#ifndef GL_TEXTURE_RECTANGLE_EXT
#define GL_TEXTURE_RECTANGLE_EXT          0x84F5
#endif

/* This is needed for building the X server: */
/*
 * GL_MESA_packed_depth_stencil
 */
#ifndef GL_MESA_packed_depth_stencil
#define GL_MESA_packed_depth_stencil 1

#define GL_DEPTH_STENCIL_MESA                   0x8750
#define GL_UNSIGNED_INT_24_8_MESA               0x8751
#define GL_UNSIGNED_INT_8_24_REV_MESA           0x8752
#define GL_UNSIGNED_SHORT_15_1_MESA             0x8753
#define GL_UNSIGNED_SHORT_1_15_REV_MESA         0x8754

#endif /* GL_MESA_packed_depth_stencil */

/* Various other OS projects expect to get these macros from Mesa's gl.h */
#ifndef GLAPI
#define GLAPI extern
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY
#endif

#ifndef APIENTRY
#define APIENTRY GLAPIENTRY
#endif

/* "P" suffix to be used for a pointer to a function */
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifndef GLAPIENTRYP
#define GLAPIENTRYP GLAPIENTRY *
#endif

#endif /*__X_GL_H*/
