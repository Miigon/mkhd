#include "hotkey.h"

#include "carbon.h"
#include "log.h"
#include "mkhd.h"
#include "sbuffer.h"
#include "tr_malloc.h"
#include "utils.h"

#define LRMOD_ALT 0
#define LRMOD_CMD 6
#define LRMOD_CTRL 9
#define LRMOD_SHIFT 3
#define LMOD_OFFS 1
#define RMOD_OFFS 2

static char arg[] = "-c";
static char *shell = NULL;

static uint32_t cgevent_lrmod_flag[] = {
	Event_Mask_Alt, Event_Mask_LAlt, Event_Mask_RAlt, Event_Mask_Shift,	  Event_Mask_LShift,   Event_Mask_RShift,
	Event_Mask_Cmd, Event_Mask_LCmd, Event_Mask_RCmd, Event_Mask_Control, Event_Mask_LControl, Event_Mask_RControl,
};

static uint32_t hotkey_lrmod_flag[] = {
	Hotkey_Flag_Alt,	Hotkey_Flag_LAlt,	 Hotkey_Flag_RAlt,	   Hotkey_Flag_Shift,
	Hotkey_Flag_LShift, Hotkey_Flag_RShift,	 Hotkey_Flag_Cmd,	   Hotkey_Flag_LCmd,
	Hotkey_Flag_RCmd,	Hotkey_Flag_Control, Hotkey_Flag_LControl, Hotkey_Flag_RControl,
};

static bool compare_lr_mod(struct keyevent *a, struct keyevent *b, int mod) {
	bool result =
		has_flags(a, hotkey_lrmod_flag[mod])
			? has_flags(b, hotkey_lrmod_flag[mod + LMOD_OFFS]) || has_flags(b, hotkey_lrmod_flag[mod + RMOD_OFFS]) ||
				  has_flags(b, hotkey_lrmod_flag[mod])
			: has_flags(a, hotkey_lrmod_flag[mod + LMOD_OFFS]) == has_flags(b, hotkey_lrmod_flag[mod + LMOD_OFFS]) &&
				  has_flags(a, hotkey_lrmod_flag[mod + RMOD_OFFS]) ==
					  has_flags(b, hotkey_lrmod_flag[mod + RMOD_OFFS]) &&
				  has_flags(a, hotkey_lrmod_flag[mod]) == has_flags(b, hotkey_lrmod_flag[mod]);
	return result;
}

static bool compare_fn(struct keyevent *a, struct keyevent *b) {
	return has_flags(a, Hotkey_Flag_Fn) == has_flags(b, Hotkey_Flag_Fn);
}

static bool compare_nx(struct keyevent *a, struct keyevent *b) {
	return has_flags(a, Hotkey_Flag_NX) == has_flags(b, Hotkey_Flag_NX);
}

bool compare_keyevent(struct keyevent *a, struct keyevent *b) {
	if (a->type != b->type)
		return false;

	if (a->type == Event_Key || a->type == Event_KeyDown || a->type == Event_KeyUp) {
		return compare_lr_mod(a, b, LRMOD_ALT) && compare_lr_mod(a, b, LRMOD_CMD) && compare_lr_mod(a, b, LRMOD_CTRL) &&
			   compare_lr_mod(a, b, LRMOD_SHIFT) && compare_fn(a, b) && compare_nx(a, b) && a->key == b->key;
	} else {
		return a->type == b->type;
	}
}

unsigned long hash_keyevent(struct keyevent *a) { return ((uint64_t)a->type << 32) & a->key; }

bool compare_string(char *a, char *b) {
	while (*a && *b && *a == *b) {
		++a;
		++b;
	}
	return *a == '\0' && *b == '\0';
}

unsigned long hash_string(char *key) {
	unsigned long hash = 0, high;
	while (*key) {
		hash = (hash << 4) + *key++;
		high = hash & 0xF0000000;
		if (high) {
			hash ^= (high >> 24);
		}
		hash &= ~high;
	}
	return hash;
}

static inline void fork_and_exec(const char *command) {
	int cpid = fork();
	if (cpid == 0) {
		setsid();
		char *exec[] = {shell, arg, (char *)command, NULL};
		int status_code = execvp(exec[0], exec);
		exit(status_code);
	}
}

static inline struct action *find_process_action(struct hotkey *hotkey, const char *process_name) {
	if (hotkey == NULL)
		return NULL;
	struct action *result = NULL;
	bool found = false;

	if (process_name && hotkey->process_names) { // process-specific mappings
		for (int i = 0; i < buf_len(hotkey->process_names); ++i) {
			if (same_string(process_name, hotkey->process_names[i])) {
				result = hotkey->actions[i];
				found = true;
				break;
			}
		}
	}

	if (!found)
		result = hotkey->process_default_action;

	return result;
}

static struct action action_fallthrough = {.type = Action_Fallthrough, .argument = NULL};
static struct action action_noop = {.type = Action_NoOp, .argument = NULL};
static struct action action_nocapture = {.type = Action_Nocapture, .argument = NULL};

// @pseudo_keys like @unmatched, @enter_layer, @exit_layer. See `enum keyevent_type`.
static struct action *find_pseudo_keyevent(struct layer *layer, enum keyevent_type type) {
	struct keyevent event = {.type = type};
	return find_process_action(table_find(&layer->hotkey_map, &event), NULL);
}

static void recursive_layer_pop(struct mkhd_state *mstate, int popcnt) {
	for (int i = 0; i < popcnt; i++) {
		if (mstate->layerstack_cnt == 1) {
			warn("mkhd: can not deactivate default layer. nothing was done.");
			return;
		}
		int idx = mstate->layerstack_cnt - 1;
		struct layer *top = mstate->layerstack[idx].l;
		mstate->layerstack_cnt--;
		debug("mkhd: poplayer |%s, to |%s\n", top->name, MS_CURRENT_LAYER(mstate).l->name);
		execute_action(mstate, find_pseudo_keyevent(top, Event_ExitLayer), idx);
	}
}

bool execute_action(struct mkhd_state *mstate, struct action *action, int in_layer) {
	if (action == NULL) {
		return false;
	}
	switch (action->type) {
	case Action_Fallthrough: {
		error("mkhd: execute_action(): Action_Fallthrough is not executable.\n");
		return false;
	}
	case Action_NoOp:
		return true; // capture
	case Action_Command:
		fork_and_exec(action->argument);
		ddebug("mkhd: cmd: %s\n", action->argument);
		return true; // capture
	case Action_Nocapture:
		return false; // no capture
	case Action_PushLayerOneshot:
	case Action_PushLayer: {
		// pops anything in the layer stack above the layer that triggered this Action_PushLayer
		recursive_layer_pop(mstate, mstate->layerstack_cnt - in_layer - 1);
		// push the new layer
		mstate->layerstack_cnt++;
		if (mstate->layerstack_cnt > LAYERSTACK_MAX) {
			warn("mkhd: layer stack overflow (max %d)! maybe you have a activating (->) loop in your config?\n",
				 LAYERSTACK_MAX);
			warn("mkhd: last 5 layers: ... -> |%s -> |%s -> |%s -> |%s -> |%s",
				 mstate->layerstack[LAYERSTACK_MAX - 5].l->name, mstate->layerstack[LAYERSTACK_MAX - 4].l->name,
				 mstate->layerstack[LAYERSTACK_MAX - 3].l->name, mstate->layerstack[LAYERSTACK_MAX - 2].l->name,
				 mstate->layerstack[LAYERSTACK_MAX - 1].l->name);
			return false; // no capture
		}
		struct layer *new_layer = table_find(&mstate->layer_map, action->argument);
		MS_CURRENT_LAYER(mstate) = (struct layerstack_frame){
			.l = new_layer,
			.oneshot = (action->type == Action_PushLayerOneshot),
		};
		debug("mkhd: activate %s |%s\n", (MS_CURRENT_LAYER(mstate).oneshot ? "(oneshot)" : ""), new_layer->name);
		execute_action(mstate, find_pseudo_keyevent(new_layer, Event_EnterLayer), mstate->layerstack_cnt - 1);

		return true; // capture
	}
	case Action_PopLayer:
		// `.deactivate` is relative to the current fallthrough level and pops everything above(including current).
		recursive_layer_pop(mstate, mstate->layerstack_cnt - in_layer);
		return true; // capture
	default:
		warn("mkhd: unknown action %d\n", action->type);
		return false;
	}
}

static struct action *find_keyevent_action_in_layer(struct layer *layer, struct keyevent *event,
													const char *process_name) {
	struct hotkey *hotkey = table_find(&layer->hotkey_map, event);
	struct action *action = NULL;
	if (hotkey == NULL) {
		ddebug("unmatched in layer |%s\n", layer->name);
		if (event->type == Event_KeyDown) {
			// unmatched keydown will always fallthrough and never trigger @unmatched
			action = &action_fallthrough;
		} else {
			action = find_pseudo_keyevent(layer, Event_Unmatched);
		}
	} else {
		action = find_process_action(hotkey, process_name);
	}
	return action;
}

bool find_and_exec_keyevent(struct mkhd_state *mstate, struct keyevent *event, const char *process_name) {
	int fallthrough_depth = mstate->layerstack_cnt - 1;

#define cur_layer() (mstate->layerstack[fallthrough_depth])

	ddebug("mkhd: event: type=%d key=%d flags=%d\n", event->type, event->key, event->flags);

	// current top layer before executing any action
	struct layerstack_frame *top = &cur_layer();
	int top_idx = fallthrough_depth;

	// find a layer that can process the event in the layer stack, from the top down.
	while (true) {
		struct action *action = find_keyevent_action_in_layer(cur_layer().l, event, process_name);
		ddebug("action->type = %d\n", action->type);
		if (action && action->type == Action_Fallthrough) {
			if (fallthrough_depth == 0) {
				// special case: `.fallthrough` at the lowest layer frame is the same as `.nocapture`
				action = &action_nocapture;
			} else {

				struct layer *oldlayer = cur_layer().l;
				fallthrough_depth--;
				ddebug("mkhd: .fallthrough |%s -> |%s\n", oldlayer->name, cur_layer().l->name);
				continue;
			}
		}

		// found a layer with action->type != Action_Fallthrough

		// Event_KeyDown alone won't consume a oneshot
		bool should_pop_oneshot = top->oneshot && (event->type == Event_Key || event->type == Event_KeyUp);
		if (should_pop_oneshot) {
			// if the top layer is oneshot, remove it first
			mstate->layerstack_cnt--;
			debug("mkhd: pop oneshot layer |%s\n", top->l->name);
		}
		bool res = execute_action(mstate, action, fallthrough_depth);
		if (should_pop_oneshot) {
			execute_action(mstate, find_pseudo_keyevent(top->l, Event_ExitLayer), top_idx);
		}
		return res;
	}
}

static struct hotkey *create_pseudo_key_hotkey(enum keyevent_type type, struct action *action) {
	struct hotkey *hotkey = tr_malloc(sizeof(struct hotkey));
	memset(hotkey, 0, sizeof(struct hotkey));

	hotkey->event.type = type;
	hotkey->process_default_action = action;

	return hotkey;
}

void add_hotkey_to_layer(struct layer *layer, struct hotkey *hotkey) {
	table_replace(&layer->hotkey_map, &hotkey->event, hotkey);
}

struct layer *create_new_layer(const char *name) {
	struct layer *layer = tr_malloc(sizeof(struct layer));
	memset(layer, 0, sizeof(struct layer));
	layer->name = copy_string_malloc(name);

	table_init(&layer->hotkey_map, 131, (table_hash_func)hash_keyevent, (table_compare_func)compare_keyevent);

	add_hotkey_to_layer(layer, create_pseudo_key_hotkey(Event_Unmatched, &action_fallthrough));
	add_hotkey_to_layer(layer, create_pseudo_key_hotkey(Event_EnterLayer, &action_noop));
	add_hotkey_to_layer(layer, create_pseudo_key_hotkey(Event_ExitLayer, &action_noop));

	return layer;
}

static void cgevent_lrmod_flag_to_hotkey_lrmod_flag(CGEventFlags eventflags, uint32_t *flags, int mod) {
	enum osx_event_mask mask = cgevent_lrmod_flag[mod];
	enum osx_event_mask lmask = cgevent_lrmod_flag[mod + LMOD_OFFS];
	enum osx_event_mask rmask = cgevent_lrmod_flag[mod + RMOD_OFFS];

	if ((eventflags & mask) == mask) {
		bool left = (eventflags & lmask) == lmask;
		bool right = (eventflags & rmask) == rmask;

		if (left)
			*flags |= hotkey_lrmod_flag[mod + LMOD_OFFS];
		if (right)
			*flags |= hotkey_lrmod_flag[mod + RMOD_OFFS];
		if (!left && !right)
			*flags |= hotkey_lrmod_flag[mod];
	}
}

static uint32_t cgevent_flags_to_hotkey_flags(uint32_t eventflags) {
	uint32_t flags = 0;

	cgevent_lrmod_flag_to_hotkey_lrmod_flag(eventflags, &flags, LRMOD_ALT);
	cgevent_lrmod_flag_to_hotkey_lrmod_flag(eventflags, &flags, LRMOD_CMD);
	cgevent_lrmod_flag_to_hotkey_lrmod_flag(eventflags, &flags, LRMOD_CTRL);
	cgevent_lrmod_flag_to_hotkey_lrmod_flag(eventflags, &flags, LRMOD_SHIFT);

	if ((eventflags & Event_Mask_Fn) == Event_Mask_Fn) {
		flags |= Hotkey_Flag_Fn;
	}

	return flags;
}

struct keyevent create_keyevent_from_CGEvent(CGEventRef event) {
	return (struct keyevent){.type = Event_Key,
							 .key = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode),
							 .flags = cgevent_flags_to_hotkey_flags(CGEventGetFlags(event))};
}

bool intercept_systemkey(CGEventRef event, struct keyevent *eventkey) {
	CFDataRef event_data = CGEventCreateData(kCFAllocatorDefault, event);
	const uint8_t *data = CFDataGetBytePtr(event_data);
	uint8_t key_code = data[129];
	uint8_t key_state = data[130];
	uint8_t key_stype = data[123];
	CFRelease(event_data);

	bool result = ((key_state == NX_KEYDOWN || key_state == NX_KEYUP) && (key_stype == NX_SUBTYPE_AUX_CONTROL_BUTTONS));

	if (result) {
		eventkey->type = key_state == NX_KEYUP ? Event_KeyUp : Event_KeyDown;
		eventkey->key = key_code;
		eventkey->flags = cgevent_flags_to_hotkey_flags(CGEventGetFlags(event)) | Hotkey_Flag_NX;
	}

	return result;
}

void init_shell(void) {
	if (!shell) {
		char *env_shell = getenv("SHELL");
		shell = env_shell ? env_shell : "/bin/bash";
	}
}
