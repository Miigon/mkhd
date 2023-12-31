#pragma once

#include "hashtable.h"
#include "hotkey.h"

#include <stdbool.h>

#define LAYERSTACK_MAX 5

struct mkhd_state {
	struct table layer_map;
	struct table blocklst;
	struct table alias_map;

	// keeps track of the history of `|>` layer switches.
	// persists between key presses.
	// only affected by PushLayer and PopLayer actions.
	struct layerstack_frame layerstack[LAYERSTACK_MAX];
	int layerstack_cnt;
};

#define MS_CURRENT_LAYER(mstate) ((mstate)->layerstack[(mstate)->layerstack_cnt - 1])

#define DEFAULT_LAYER "default"

void mkhd_event_tap_set_enabled(bool enabled);