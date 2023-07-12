#pragma once

// tracked malloc
// tracks every allocation in a memory context.
// then once they are all done, free them all by calling `trctx_free_everything()`
//
// this frees us from the hassle of manually managing and freeing memories (hotkeys, actions, command strings, etc) on
// config reload.
// everything is supposed to be all freed on (and only on) config reload anyway.

struct trctx;

struct trctx *trctx_new_context();
void trctx_destroy_context(struct trctx *ctx);

void *trctx_malloc(struct trctx *ctx, int sz);
void trctx_free(void *ptr);
void *trctx_realloc(struct trctx *ctx, void *ptr, int sz);

int trctx_free_everything(struct trctx *ctx);
int trctx_reclaim_empty_slots(struct trctx *ctx);

extern struct trctx *trctx_g_ctx; // global context. use `trctx_set_memcontext()` to set.

// these are shorthand version of tracked mallocs that uses the global memory context.
// set a global memory context before using these.

#define tr_malloc(sz) trctx_malloc(trctx_g_ctx, sz)
#define tr_free(ptr) trctx_free(ptr)
#define tr_realloc(ptr, sz) trctx_realloc(trctx_g_ctx, ptr, sz)

// returns the original context
struct trctx *trctx_set_memcontext(struct trctx *ctx);