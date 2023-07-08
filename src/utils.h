#pragma once
#include <stdbool.h>

bool same_string(const char *a, const char *b);
char *copy_string_malloc(const char *s);
char *file_directory(char *file);
char *copy_string_count_nomalloc(char *dst, const char *s, int length);
char *copy_string_count_malloc(const char *s, int length);
char *file_name(char *file);

#define array_count(a) (sizeof((a)) / sizeof(*(a)))