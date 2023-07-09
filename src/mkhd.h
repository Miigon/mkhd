#pragma once
#include "hashtable.h"

#define LAYERSTACK_MAX 5

struct layer;
struct mkhd_state {
	struct table layer_map;
	struct table blacklst;
	struct table alias_map;

	// keeps track of the history of `|>` layer switches.
	// persists between key presses.
	// only affected by PushLayer and PopLayer actions.
	struct layer *layerstack[LAYERSTACK_MAX];
	int layerstack_cnt;
};

#define MS_CURRENT_LAYER(mstate) ((mstate)->layerstack[(mstate)->layerstack_cnt - 1])

#define DEFAULT_LAYER "default"