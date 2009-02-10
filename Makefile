INSTALL_DIR = /usr/X11
X11_DIR = $(INSTALL_DIR)

CC=gcc
CFLAGS=-Wall -ggdb3 -Os -DPTHREADS -D_REENTRANT -DPUBLIC="" $(RC_CFLAGS)
LDFLAGS=-L$(X11_DIR)/lib

MKDIR=mkdir
INSTALL=install
LN=ln
RM=rm

INCLUDE=-I. -Iinclude -Iinclude/internal -DGLX_ALIAS_UNSUPPORTED -Igl
COMPILE=$(CC) $(CFLAGS) $(INCLUDE) -c

TEST_BUILD_DIR=builds

all: $(TEST_BUILD_DIR) Makefile libGL.1.2.dylib libGL.dylib tests

include tests/tests.mk

OBJECTS=glxext.o glxcmds.o glx_pbuffer.o glx_query.o glxcurrent.o glxextensions.o \
    appledri.o apple_glx_context.o apple_glx.o pixel.o \
    compsize.o apple_visual.o apple_cgl.o glxreply.o glcontextmodes.o \
    apple_xgl_api.o apple_glx_drawable.o xfont.o apple_glx_pbuffer.o

#This target is used for the tests.

$(TEST_BUILD_DIR):
	$(MKDIR) $(TEST_BUILD_DIR)

libGL.dylib: $(OBJECTS)
	$(CC) -o libGL.dylib -dynamiclib -lXplugin -framework ApplicationServices -framework CoreFoundation -L$(X11_DIR)/lib -lX11 -lXext -Wl,-exported_symbols_list,exports.list $(OBJECTS)

libGL.1.2.dylib: $(OBJECTS)
	$(CC) $(CFLAGS) -o libGL.1.2.dylib -dynamiclib -install_name $(INSTALL_DIR)/lib/libGL.1.2.dylib -compatibility_version 1.2 -current_version 1.2 -lXplugin -framework ApplicationServices -framework CoreFoundation $(LDFLAGS) -lXext -lX11 -Wl,-exported_symbols_list,exports.list $(OBJECTS)

apple_glx_drawable.o: apple_glx_drawable.h apple_glx_drawable.c
	$(COMPILE) apple_glx_drawable.c

apple_xgl_api.o: apple_xgl_api.h apple_xgl_api.c
	$(COMPILE) apple_xgl_api.c

glcontextmodes.o: glcontextmodes.c glcontextmodes.h
	$(COMPILE) glcontextmodes.c

glxext.o: glxext.c
	$(COMPILE) glxext.c

glxreply.o: glxreply.c
	$(COMPILE) glxreply.c

glxcmds.o: glxcmds.c apple_glx_context.h
	$(COMPILE) glxcmds.c

glx_pbuffer.o: glx_pbuffer.c
	$(COMPILE) glx_pbuffer.c

glx_query.o: glx_query.c
	$(COMPILE) glx_query.c

glxcurrent.o: glxcurrent.c
	$(COMPILE) glxcurrent.c

glxextensions.o: glxextensions.h glxextensions.c
	$(COMPILE) glxextensions.c

glxhash.o: glxhash.h glxhash.c
	$(COMPILE) glxhash.c

appledri.o: appledri.h appledristr.h appledri.c
	$(COMPILE) -I$(X11_DIR)/include/X11 appledri.c

apple_glx_context.o: apple_glx_context.c apple_glx_context.h apple_glx_context.h
	$(COMPILE) apple_glx_context.c -F/System/Library/Frameworks/OpenGL.framework

apple_glx.o: apple_glx.h apple_glx.c
	$(COMPILE) -Iinclude apple_glx.c -F/System/Library/Frameworks/OpenGL.framework

apple_visual.o: apple_visual.h apple_visual.c
	$(COMPILE) -Iinclude apple_visual.c -F/System/Library/Frameworks/OpenGL.framework

#apple_api.o: apple_api.h apple_api.c
#	$(COMPILE) -Iinclude apple_api.c -F/System/Library/Frameworks/OpenGL.framework

apple_cgl.o: apple_cgl.h apple_cgl.c
	$(COMPILE) -Iinclude apple_cgl.c 

apple_glx_pbuffer.o: apple_glx_pbuffer.h apple_glx_pbuffer.c
	$(COMPILE) -Iinclude apple_glx_pbuffer.c

xfont.o: xfont.c glxclient.h
	$(COMPILE) xfont.c

compsize.o: compsize.c
	$(COMPILE) $?

renderpix.o: renderpix.c
	$(COMPILE) $?

singlepix.o: singlepix.c
	$(COMPILE) $?

pixel.o: pixel.c
	$(COMPILE) $?

install: libGL.1.2.dylib
	$(INSTALL) -d $(DESTDIR)$(INSTALL_DIR)/lib
	$(INSTALL) -m 755 libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib
	$(RM) -f $(DESTDIR)$(INSTALL_DIR)/lib/libGL.dylib
	$(LN) -s libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib/libGL.dylib
	$(RM) -f $(DESTDIR)$(INSTALL_DIR)/lib/libGL.1.dylib
	$(LN) -s libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib/libGL.1.dylib

clean:
	rm -rf $(TEST_BUILD_DIR)
	rm -f *.o *.a
	rm -f *.c~ *.h~
	rm -f *.dylib
