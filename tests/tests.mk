.PHONY : tests

LIBGL=./libGL.dylib
LINK_TEST=-L/usr/X11/lib ./libGL.dylib -lX11 -lXext -lXplugin -lpthread

include tests/triangle/triangle.mk
include tests/simple/simple.mk
include tests/fbconfigs/fbconfigs.mk
include tests/triangle_glx/triangle_glx.mk
include tests/create_destroy_context/create_destroy_context.mk
include tests/glxgears/glxgears.mk
include tests/glxinfo/glxinfo.mk
include tests/pbuffer/pbuffer.mk
include tests/texenv/texenv.mk
include tests/engine/engine.mk
include tests/glxpixmap/glxpixmap.mk
include tests/triangle_glx_single/triangle_glx.mk

tests: $(TEST_BUILD_DIR)/simple $(TEST_BUILD_DIR)/fbconfigs $(TEST_BUILD_DIR)/triangle_glx \
  $(TEST_BUILD_DIR)/create_destroy_context $(TEST_BUILD_DIR)/glxgears $(TEST_BUILD_DIR)/glxinfo \
  $(TEST_BUILD_DIR)/pbuffer $(TEST_BUILD_DIR)/pbuffer_destroy \
  $(TEST_BUILD_DIR)/glxpixmap \
  $(TEST_BUILD_DIR)/triangle_glx_single

