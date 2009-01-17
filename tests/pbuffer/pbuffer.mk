$(TEST_BUILD_DIR)/pbuffer: tests/pbuffer/pbuffer.c $(LIBGL)
	$(CC) tests/pbuffer/pbuffer.c -Iinclude -o $(TEST_BUILD_DIR)/pbuffer $(LINK_TEST)
