$(TEST_BUILD_DIR)/texenv: tests/texenv/texenv.c $(LIBGL)
	$(CC) tests/texenv/texenv.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/texenv -L/usr/X11/lib -lXmu -lglu libglut.a $(LINK_TEST)
