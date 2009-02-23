triangle: tests/triangle/triangle.m libgl.a libglx.a
	$(CC) tests/triangle/triangle.m $(INCLUDE) -L$(INSTALL_DIR)/lib -L$(X11_DIR)/lib -lglut libgl.a libglx.a -o triangle \
-F/System/Library/Frameworks/OpenGL.framework /System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib
