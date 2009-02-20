$(TEST_BUILD_DIR)/sharedtex: tests/shared/sharedtex.c $(LIBGL)
	$(CC) tests/shared/sharedtex.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/sharedtex $(LINK_TEST)

