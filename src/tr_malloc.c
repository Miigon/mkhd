#include "tr_malloc.h"

#include "log.h"

#include <stdlib.h>

#define MAX_TRACKED_OBJECTS 16384

struct trctx {
	void *slots[MAX_TRACKED_OBJECTS];
	int tracked_cnt;
};

struct tracked_mem_header {
	struct trctx *ctx;
	void **slot_ref;
};

#define HDR_OFFSET (sizeof(struct tracked_mem_header))
#define PTR_HDR(ptr) ((struct tracked_mem_header *)(ptr))

struct trctx *trctx_new_context() {
	struct trctx *ctx = malloc(sizeof(struct trctx));
	memset(ctx, 0, sizeof(struct trctx));
	return ctx;
}

void trctx_destroy_context(struct trctx *ctx) {
	trctx_free_everything(ctx);
	free(ctx);
}

void *trctx_malloc(struct trctx *ctx, int sz) {
	if (sz == 0)
		return NULL;
	void *ptr = malloc(sz + HDR_OFFSET);

	ctx->tracked_cnt++;
	if (ctx->tracked_cnt > MAX_TRACKED_OBJECTS) {
		error("mkhd: too many objects allocated within memory context (max %d)\n"
			  "mkhd: this might indicate a config that is too large.\n"
			  "mkhd: if this is not the case, please file a bug report!\n",
			  MAX_TRACKED_OBJECTS);
	}
	ctx->slots[ctx->tracked_cnt - 1] = ptr;
	PTR_HDR(ptr)->ctx = ctx;
	PTR_HDR(ptr)->slot_ref = &ctx->slots[ctx->tracked_cnt - 1];

	return ptr + HDR_OFFSET;
}

void trctx_free(void *ptr) {
	if (ptr == NULL)
		return;
	// set the slot it takes to NULL
	// the slot still can't be used to hold new object unless trctx_reclaim_empty_slots() is called
	ptr = ptr - HDR_OFFSET;
	*PTR_HDR(ptr)->slot_ref = NULL;
	// keep ctx->tracked_cnt unchanged.
	free(ptr);
}

void *trctx_realloc(struct trctx *ctx, void *ptr, int sz) {
	if (ptr == NULL)
		return trctx_malloc(ctx, sz);
	ptr = ptr - HDR_OFFSET;
	ptr = realloc(ptr, sz + HDR_OFFSET);
	PTR_HDR(ptr)->slot_ref = ptr;
	if (PTR_HDR(ptr)->ctx != ctx) {
		error("mkhd: trctx_realloc: try to realloc object from another memory context\n");
	}

	return ptr + HDR_OFFSET;
}

int trctx_free_everything(struct trctx *ctx) {
	int freed_objects = 0;
	for (int i = 0; i < ctx->tracked_cnt; i++) {
		if (ctx->slots[i] == NULL)
			continue;
		free(ctx->slots[i]);
		freed_objects++;
		ctx->slots[i] = NULL;
	}
	ctx->tracked_cnt = 0;
	return freed_objects;
}

// removes NULL entries in ctx->slots
int trctx_reclaim_empty_slots(struct trctx *ctx) {
	int new_tracked_cnt = 0;
	for (int i = 0; i < ctx->tracked_cnt; i++) {
		if (ctx->slots[i] == NULL)
			continue;
		new_tracked_cnt++;
		void *ptr = ctx->slots[new_tracked_cnt - 1] = ctx->slots[i];
		PTR_HDR(ptr)->slot_ref = &ctx->slots[new_tracked_cnt - 1];

		if (new_tracked_cnt - 1 != i) {
			ctx->slots[i] = NULL;
		}
	}
	ctx->tracked_cnt = new_tracked_cnt;
	return new_tracked_cnt;
}

struct trctx *trctx_g_ctx = NULL;

void trctx_set_memcontext(struct trctx *ctx) { trctx_g_ctx = ctx; }