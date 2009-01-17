$(TEST_BUILD_DIR)/glxinfo: tests/glxinfo/glxinfo.c $(LIBGL)
	$(CC) tests/glxinfo/glxinfo.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/glxinfo $(LINK_TEST)
