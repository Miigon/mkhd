#pragma once
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool verbose;
extern bool veryverbose;

static inline void ddebug(const char *format, ...) {
	if (!veryverbose)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
}

static inline void debug(const char *format, ...) {
	if (!verbose)
		return;

	va_list args;
	va_start(args, format);
	vfprintf(stdout, format, args);
	va_end(args);
}

static inline void warn(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}

static inline void error(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

static inline void require(const char *format, ...) {
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	exit(EXIT_SUCCESS);
}
