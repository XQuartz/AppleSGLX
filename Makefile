INSTALL_DIR = /usr/X11
X11_DIR = $(INSTALL_DIR)

CC=gcc
GL_CFLAGS=-Wall -ggdb3 -Os -DPTHREADS -D_REENTRANT -DPUBLIC="" $(RC_CFLAGS) $(CFLAGS)
GL_LDFLAGS=-L$(X11_DIR)/lib $(LDFLAGS)

MKDIR=mkdir
INSTALL=install
LN=ln
RM=rm

INCLUDE=-I. -Iinclude -Iinclude/internal -DGLX_ALIAS_UNSUPPORTED -F/System/Library/Frameworks/OpenGL.framework -I$(INSTALL_DIR)/include
COMPILE=$(CC) $(INCLUDE) $(GL_CFLAGS) -c

#The directory with the final binaries.
BUILD_DIR=builds

#The directory with binaries that can tested without an install.
TEST_BUILD_DIR=testbuilds

PROGRAMS=$(BUILD_DIR)/glxinfo $(BUILD_DIR)/glxgears

all: $(PROGRAMS) tests

include tests/tests.mk

OBJECTS=glxext.o glxcmds.o glx_pbuffer.o glx_query.o glxcurrent.o glxextensions.o \
    appledri.o apple_glx_context.o apple_glx.o pixel.o \
    compsize.o apple_visual.o apple_cgl.o glxreply.o glcontextmodes.o \
    apple_xgl_api.o apple_glx_drawable.o xfont.o apple_glx_pbuffer.o \
    apple_glx_pixmap.o apple_xgl_api_read.o glx_empty.o

#This target is used for the tests.
$(TEST_BUILD_DIR):
	$(MKDIR) $(TEST_BUILD_DIR)

$(BUILD_DIR):
	$(MKDIR) $(BUILD_DIR)

#This is used for building the tests.
#The tests don't require installation.
$(TEST_BUILD_DIR)/libGL.dylib: $(TEST_BUILD_DIR) $(OBJECTS)
	$(CC) -o $(TEST_BUILD_DIR)/libGL.dylib -dynamiclib -lXplugin -framework ApplicationServices -framework CoreFoundation -L$(X11_DIR)/lib -lX11 -lXext -Wl,-exported_symbols_list,exports.list $(OBJECTS)

$(BUILD_DIR)/libGL.1.2.dylib: $(BUILD_DIR) $(OBJECTS)
	$(CC) $(GL_CFLAGS) -o $(BUILD_DIR)/libGL.1.2.dylib -dynamiclib -install_name $(INSTALL_DIR)/lib/libGL.1.2.dylib -compatibility_version 1.2 -current_version 1.2 -lXplugin -framework ApplicationServices -framework CoreFoundation $(GL_LDFLAGS) -lXext -lX11 -Wl,-exported_symbols_list,exports.list $(OBJECTS)

apple_glx_drawable.o: apple_glx_drawable.h apple_glx_drawable.c apple_glx_pixmap.h apple_glx_pbuffer.h
	$(COMPILE) apple_glx_drawable.c

apple_xgl_api.o: apple_xgl_api.h apple_xgl_api.c apple_xgl_api_stereo.c
	$(COMPILE) apple_xgl_api.c

apple_xgl_api_read.o: apple_xgl_api_read.h apple_xgl_api_read.c apple_xgl_api.h
	$(COMPILE) apple_xgl_api_read.c

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
	$(COMPILE) appledri.c

apple_glx_context.o: apple_glx_context.c apple_glx_context.h apple_glx_context.h
	$(COMPILE) apple_glx_context.c

apple_glx.o: apple_glx.h apple_glx.c
	$(COMPILE) apple_glx.c

apple_visual.o: apple_visual.h apple_visual.c
	$(COMPILE) apple_visual.c

apple_cgl.o: apple_cgl.h apple_cgl.c
	$(COMPILE) apple_cgl.c 

apple_glx_pbuffer.o: apple_glx_pbuffer.h apple_glx_pbuffer.c
	$(COMPILE) apple_glx_pbuffer.c

apple_glx_pixmap.o: apple_glx_pixmap.h apple_glx_pixmap.c appledri.h
	$(COMPILE) apple_glx_pixmap.c

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

glx_empty.o: glx_empty.c
	$(COMPILE) glx_empty.c

$(BUILD_DIR)/glxinfo: tests/glxinfo/glxinfo.c $(BUILD_DIR)/libGL.1.2.dylib
	$(CC) tests/glxinfo/glxinfo.c -I$(DESTDIR)$(INSTALL_DIR)/include -L$(BUILD_DIR) -L$(X11_DIR)/lib -lX11 -lGL -o $(BUILD_DIR)/glxinfo

$(BUILD_DIR)/glxgears: tests/glxgears/glxgears.c $(BUILD_DIR)/libGL.1.2.dylib
	$(CC) tests/glxgears/glxgears.c -I$(DESTDIR)$(INSTALL_DIR)/include -L$(BUILD_DIR) -L$(X11_DIR)/lib -lX11 -lGL -o $(BUILD_DIR)/glxgears

install_headers:
	$(INSTALL) -d $(DESTDIR)$(INSTALL_DIR)/include/GL
	$(INSTALL) -m 444 include/GL/glx.h include/GL/glxext.h include/GL/glxint.h include/GL/glxmd.h include/GL/glxproto.h $(DESTDIR)$(INSTALL_DIR)/include/GL

install_programs: $(PROGRAMS)
	$(INSTALL) -d $(DESTDIR)$(INSTALL_DIR)/bin
	$(INSTALL) -m 555 $(PROGRAMS) $(DESTDIR)$(INSTALL_DIR)/bin

install_libraries: $(BUILD_DIR)/libGL.1.2.dylib
	$(INSTALL) -d $(DESTDIR)$(INSTALL_DIR)/lib
	$(INSTALL) -m 755 $(BUILD_DIR)/libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib
	$(RM) -f $(DESTDIR)$(INSTALL_DIR)/lib/libGL.dylib
	$(LN) -s libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib/libGL.dylib
	$(RM) -f $(DESTDIR)$(INSTALL_DIR)/lib/libGL.1.dylib
	$(LN) -s libGL.1.2.dylib $(DESTDIR)$(INSTALL_DIR)/lib/libGL.1.dylib

install: install_headers install_libraries install_programs

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(TEST_BUILD_DIR)
	rm -f *.o *.a
	rm -f *.c~ *.h~
	rm -f *.dylib
