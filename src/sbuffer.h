#pragma once

#include <stdint.h>
#include <stdlib.h>

#include "tr_malloc.h"

struct buf_hdr {
	size_t len;
	size_t cap;
	char buf[0];
};

#define buf_MAX(a, b) ((a) > (b) ? (a) : (b))
#define buf_OFFSETOF(t, f) (size_t)((char *)&(((t *)0)->f) - (char *)0)

#define buf__hdr(b) ((struct buf_hdr *)((char *)(b)-buf_OFFSETOF(struct buf_hdr, buf)))
#define buf__should_grow(b, n) (buf_len(b) + (n) >= buf_cap(b))
#define buf__fit(b, n) (buf__should_grow(b, n) ? ((b) = buf__grow_f(b, buf_len(b) + (n), sizeof(*(b)))) : 0)

#define buf_len(b) ((b) ? buf__hdr(b)->len : 0)
#define buf_cap(b) ((b) ? buf__hdr(b)->cap : 0)
#define buf_push(b, x) (buf__fit(b, 1), (b)[buf_len(b)] = (x), buf__hdr(b)->len++)
#define buf_last(b) ((b)[buf_len(b) - 1])
#define buf_free(b) ((b) ? tr_free(buf__hdr(b)) : 0)

inline static void *buf__grow_f(const void *buf, size_t new_len, size_t elem_size) {
	size_t new_cap = buf_MAX(1 + 2 * buf_cap(buf), new_len);
	size_t new_size = buf_OFFSETOF(struct buf_hdr, buf) + new_cap * elem_size;
	struct buf_hdr *new_hdr = (struct buf_hdr *)tr_realloc(buf ? buf__hdr(buf) : 0, new_size);
	new_hdr->cap = new_cap;
	if (!buf) {
		new_hdr->len = 0;
	}
	return new_hdr->buf;
}
