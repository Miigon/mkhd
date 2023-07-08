#include "utils.h"

#include <stdlib.h>
#include <string.h>

bool same_string(const char *a, const char *b) {
	bool result = a && b && strcmp(a, b) == 0;
	return result;
}

char *copy_string_malloc(const char *s) {
	unsigned length = strlen(s);
	char *result = (char *)malloc(length + 1);

	copy_string_count_nomalloc(result, s, length);

	return result;
}

char *copy_string_count_nomalloc(char *dst, const char *s, int length) {
	memcpy(dst, s, length);
	dst[length] = '\0';
	return dst;
}

char *copy_string_count_malloc(const char *s, int length) {
	char *result = malloc(length + 1);
	return copy_string_count_nomalloc(result, s, length);
}

char *file_directory(char *file) {
	char *last_slash = strrchr(file, '/');
	*last_slash = '\0';
	char *directory = copy_string_malloc(file);
	*last_slash = '/';
	return directory;
}

char *file_name(char *file) {
	char *last_slash = strrchr(file, '/');
	char *name = copy_string_malloc(last_slash + 1);
	return name;
}
