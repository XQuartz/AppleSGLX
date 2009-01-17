$(TEST_BUILD_DIR)/glxpixmap: tests/glxpixmap/glxpixmap.c $(LIBGL)
	$(CC) tests/glxpixmap/glxpixmap.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/glxpixmap $(LINK_TEST)
