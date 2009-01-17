$(TEST_BUILD_DIR)/fbconfigs: tests/fbconfigs/fbconfigs.c $(LIBGL)
	$(CC) tests/fbconfigs/fbconfigs.c -Iinclude -I/usr/X11/include -o $(TEST_BUILD_DIR)/fbconfigs $(LINK_TEST)
