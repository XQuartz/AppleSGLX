$(TEST_BUILD_DIR)/triangle_glx: tests/triangle_glx/triangle_glx.c $(LIBGL)
	$(CC) tests/triangle_glx/triangle_glx.c -Iinclude -o $(TEST_BUILD_DIR)/triangle_glx $(LINK_TEST)

$(TEST_BUILD_DIR)/triangle_glx_surface: tests/triangle_glx/triangle_glx_surface.c $(LIBGL)
	$(CC) tests/triangle_glx/triangle_glx_surface.c -Iinclude -o $(TEST_BUILD_DIR)/triangle_glx_surface $(LINK_TEST)

$(TEST_BUILD_DIR)/triangle_glx_surface-2: tests/triangle_glx/triangle_glx_surface-2.c $(LIBGL)
	$(CC) tests/triangle_glx/triangle_glx_surface-2.c -Iinclude -o $(TEST_BUILD_DIR)/triangle_glx_surface-2 $(LINK_TEST)
