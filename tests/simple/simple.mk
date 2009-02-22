$(TEST_BUILD_DIR)/simple: tests/simple/simple.c $(LIBGL)
	$(CC) tests/simple/simple.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/simple $(LINK_TEST)

$(TEST_BUILD_DIR)/render_types: tests/simple/render_types.c $(LIBGL)
	$(CC) tests/simple/render_types.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/render_types $(LINK_TEST)

$(TEST_BUILD_DIR)/drawable_types: tests/simple/drawable_types.c $(LIBGL)
	$(CC) tests/simple/drawable_types.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/drawable_types $(LINK_TEST)

$(TEST_BUILD_DIR)/multisample_glx: tests/simple/multisample_glx.c $(LIBGL)
	$(CC) tests/simple/multisample_glx.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/multisample_glx $(LINK_TEST)

$(TEST_BUILD_DIR)/glthreads: tests/simple/glthreads.c $(LIBGL)
	$(CC) -DPTHREADS -pthread tests/simple/glthreads.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/glthreads $(LINK_TEST)

