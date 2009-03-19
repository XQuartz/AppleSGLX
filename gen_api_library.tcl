package require Tcl 8.5

set license {
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
}

set gl_license {
/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
** 
** http://oss.sgi.com/projects/FreeB
** 
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
** 
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
** 
** Additional Notice Provisions: This software was created using the
** OpenGL(R) version 1.2.1 Sample Implementation published by SGI, but has
** not been independently verified as being compliant with the OpenGL(R)
** version 1.2.1 Specification.
*/
}

set init_code {
static void *glsym(void *handle, const char *name) { 
    void *sym = dlsym(handle, name);

    if(NULL == sym) {
	fprintf(stderr, "Error: symbol not found: '%s'.  "
		"Error information: %s\n",
		name, dlerror());
	abort();
    }

    return sym;
}

}

set dlopen_code {
#ifndef LIBGLNAME
#define LIBGLNAME "/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib"
#endif LIBGLNAME

    (void)dlerror(); /*drain dlerror()*/

    handle = dlopen(LIBGLNAME, RTLD_LAZY);
    
    if(NULL == handle) {
	fprintf(stderr, "error: unable to dlopen "
		LIBGLNAME " :" "%s\n", dlerror());
	abort();
    }
}

set this_script [info script]

proc main {argc argv} {
    if {2 != $argc} {
	puts stderr "syntax is: [set ::this_script] serialized-array-file output.c"
	return 1
    }

    
    set fd [open [lindex $argv 0] r]
    array set api [read $fd]
    close $fd

    set fd [open [lindex $argv 1] w]
    
    puts $fd "/* This file was automatically generated by [set ::this_script]. */"
    puts $fd $::license

    puts $fd {
#include <dlfcn.h>
#include "glxclient.h"
#include "apple_xgl_api.h"
#include "apple_glx_context.h"
    }

    puts $fd "struct apple_xgl_api __gl_api;"
    
    set sorted [lsort -dictionary [array names api]]
    
    set exclude [list DrawBuffer DrawBuffers DrawBuffersARB]

    #These are special to glXMakeContextCurrent.
    #See also: apple_xgl_api_read.c.    
    lappend exclude ReadPixels CopyPixels CopyColorTable 
    
    #This is excluded to work with surface updates.
    lappend exclude Viewport
    
    foreach f $sorted {
	if {$f in $exclude} {
	    continue
	}

	set attr $api($f)

        set pstr ""

        foreach p [dict get $attr parameters] {
            append pstr "[lindex $p 0] [lindex $p 1], "
        }

	set pstr [string trimright $pstr ", "]

	if {![string length $pstr]} {
	    set pstr void
	}

	set callvars ""

	foreach p [dict get $attr parameters] {
	    append callvars "[lindex $p end], "
	}

	set callvars [string trimright $callvars ", "]

	set return ""
	if {"void" ne [dict get $attr return]} {
	    set return "return "
	}

	if {[dict exists $attr noop]} {
	    if {"void" eq [dict get $attr return]} {
		set body "/*noop*/"
	    } else {
		set body "return 0; /*noop*/"
	    }
	} elseif {[dict exists $attr alias_for]} {
	    set alias [dict get $attr alias_for]
	    set body "[set return] gl[set alias]([set callvars]);"
	} else {
	    set body "[set return]__gl_api.[set f]([set callvars]);"
	}

        puts $fd "[dict get $attr return] gl[set f]([set pstr]) \{\n\t$body\n\}"
    }

    puts $fd $::init_code
    
    puts $fd "void apple_xgl_init_direct(void) \{"
    puts $fd "\tvoid *handle;"

    puts $fd $::dlopen_code
    
    foreach f $sorted {
	set attr $api($f)

	puts $attr
	puts $f

	if {[dict exists $attr alias_for] || [dict exists $attr noop]} {
	    #Function f is an alias_for another, so we shouldn't try
	    #to load it.
	    continue
	}

	puts $fd "\t__gl_api.$f = glsym(handle, \"gl$f\");"
    }
    
    puts $fd "\}\n"
    close $fd

    return 0
}
exit [main $::argc $::argv]
