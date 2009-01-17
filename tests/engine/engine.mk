$(TEST_BUILD_DIR)/engine: tests/engine/engine.c $(LIBGL) libglut.a
	$(CC) tests/engine/engine.c tests/engine/readtex.c tests/engine/trackball.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/engine $(LINK_TEST) libglut.a -L/usr/X11/lib -lXmu -lGLU
