#pragma once
#include "hashtable.h"

#define MODESTACK_MAX 256

struct mode;
struct mkhd_state {
	struct table mode_map;
	struct table blacklst;
	struct table alias_map;

	struct mode *modestack[MODESTACK_MAX];
	int modestack_cnt;
};

#define MS_CURRENT_MODE(mstate) ((mstate)->modestack[(mstate)->modestack_cnt - 1])

#define DEFAULT_MODE "default"