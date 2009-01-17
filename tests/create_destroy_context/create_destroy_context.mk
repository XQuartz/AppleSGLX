$(TEST_BUILD_DIR)/create_destroy_context: tests/create_destroy_context/create_destroy_context.c
	$(CC) tests/create_destroy_context/create_destroy_context.c -Iinclude \
    -o $(TEST_BUILD_DIR)/create_destroy_context $(LINK_TEST)
