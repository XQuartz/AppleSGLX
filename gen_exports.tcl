if 0 { 
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
}

package require Tcl 8.5

proc main {argc argv} {
    if {2 != $argc} {
	puts stderr "syntax is: [info script] serialized-array-file export.list"
	return 1
    }

    set fd [open [lindex $argv 0] r]
    array set api [read $fd]
    close $fd

    #Start with 1.0
    set glxlist [list \
                     glXChooseVisual glXCreateContext glXDestroyContext \
                     glXMakeCurrent glXCopyContext glXSwapBuffers \
                     glXCreateGLXPixmap glXDestroyGLXPixmap \
                     glXQueryExtension glXQueryVersion \
                     glXIsDirect glXGetConfig \
                     glXGetCurrentContext glXGetCurrentDrawable \
                     glXWaitGL glXWaitX glXUseXFont]

    #GLX 1.1 and later
    lappend glxlist glXQueryExtensionsString glXQueryServerString \
                     glXGetClientString

    #GLX 1.2 and later
    lappend glxlist glXGetCurrentDisplay

    #GLX 1.3 and later
    lappend glxlist glXChooseFBConfig glXGetFBConfigAttrib \
        glXGetFBConfigs glXGetVisualFromFBConfig \
        glXCreateWindow glXDestroyWindow \
        glXCreatePixmap glXDestroyPixmap \
        glXCreatePbuffer glXDestroyPbuffer \
        glXQueryDrawable glXCreateNewContext \
        glXMakeContextCurrent glXGetCurrentReadDrawable \
        glXQueryContext glXSelectEvent glXGetSelectedEvent

    #GLX 1.4 and later
    lappend glxlist glXGetProcAddress

    #Extensions
    lappend glxlist glXGetProcAddressARB

    #Old extensions we don't support and never really have, but need for
    #symbol compatibility.  See also: glx_empty.c
    lappend glxlist glXSwapIntervalSGI glXSwapIntervalMESA \
	glXGetSwapIntervalMESA glXBeginFrameTrackingMESA \
	glXEndFrameTrackingMESA glXGetFrameUsageMESA \
	glXQueryFrameTrackingMESA glXGetVideoSyncSGI \
	glXWaitVideoSyncSGI glXJoinSwapGroupSGIX \
	glXBindSwapBarrierSGIX glXQueryMaxSwapBarriersSGIX \
	glXGetSyncValuesOML glXSwapBuffersMscOML \
	glXWaitForMscOML glXWaitForSbcOML \
	glXAllocateMemoryMESA glXFreeMemoryMESA \
	glXGetMemoryOffsetMESA glXReleaseBuffersMESA \
	glXCreateGLXPixmapMESA glXCopySubBufferMESA
    

    set fd [open [lindex $argv 1] w]
    
    foreach f [lsort -dictionary [array names api]] {
	puts $fd _gl$f
    }

    foreach f [lsort -dictionary $glxlist] {
	puts $fd _$f
    }
    
    close $fd

    return 0
}

exit [main $::argc $::argv]