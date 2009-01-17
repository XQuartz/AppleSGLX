triangle: tests/triangle/triangle.m libgl.a libglx.a
	$(CC) tests/triangle/triangle.m -Iinclude -I/usr/X11/include -L/usr/X11/lib -lglut libgl.a libglx.a -o triangle \
-F/System/Library/Frameworks/OpenGL.framework /System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib
