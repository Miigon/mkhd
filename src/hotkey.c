#include "hotkey.h"

#include "carbon.h"
#include "log.h"
#include "sbuffer.h"
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
	Hotkey_Flag_Alt, Hotkey_Flag_LAlt, Hotkey_Flag_RAlt, Hotkey_Flag_Shift,	  Hotkey_Flag_LShift,	Hotkey_Flag_RShift,
	Hotkey_Flag_Cmd, Hotkey_Flag_LCmd, Hotkey_Flag_RCmd, Hotkey_Flag_Control, Hotkey_Flag_LControl, Hotkey_Flag_RControl,
};

static bool compare_lr_mod(struct keyevent *a, struct keyevent *b, int mod) {
	bool result =
		has_flags(a, hotkey_lrmod_flag[mod])
			? has_flags(b, hotkey_lrmod_flag[mod + LMOD_OFFS]) || has_flags(b, hotkey_lrmod_flag[mod + RMOD_OFFS]) || has_flags(b, hotkey_lrmod_flag[mod])
			: has_flags(a, hotkey_lrmod_flag[mod + LMOD_OFFS]) == has_flags(b, hotkey_lrmod_flag[mod + LMOD_OFFS]) &&
				  has_flags(a, hotkey_lrmod_flag[mod + RMOD_OFFS]) == has_flags(b, hotkey_lrmod_flag[mod + RMOD_OFFS]) &&
				  has_flags(a, hotkey_lrmod_flag[mod]) == has_flags(b, hotkey_lrmod_flag[mod]);
	return result;
}

static bool compare_fn(struct keyevent *a, struct keyevent *b) { return has_flags(a, Hotkey_Flag_Fn) == has_flags(b, Hotkey_Flag_Fn); }

static bool compare_nx(struct keyevent *a, struct keyevent *b) { return has_flags(a, Hotkey_Flag_NX) == has_flags(b, Hotkey_Flag_NX); }

bool compare_keyevent(struct keyevent *a, struct keyevent *b) {
	return compare_lr_mod(a, b, LRMOD_ALT) && compare_lr_mod(a, b, LRMOD_CMD) && compare_lr_mod(a, b, LRMOD_CTRL) && compare_lr_mod(a, b, LRMOD_SHIFT) &&
		   compare_fn(a, b) && compare_nx(a, b) && a->key == b->key;
}

unsigned long hash_keyevent(struct keyevent *a) { return a->key; }

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
	struct action *result = NULL;
	bool found = false;

	if (hotkey->process_names) { // process-specific mappings
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

bool execute_action(struct mkhd_state *mstate, struct action *action) {
	int fallthrough_depth = mstate->layerstack_cnt - 1;

#define cur_layer() (mstate->layerstack[fallthrough_depth])

	// do action
	while (true) {
		switch (action->type) {
		// === non-terminating actions ===
		case Action_Fallthrough: {
			if (fallthrough_depth == 0) {
				// special case: `.fallthrough` at the lowest layer frame is the same as `.nocapture`
				return false;
			}
			struct layer *oldlayer = cur_layer();
			fallthrough_depth--;
			debug("mkhd: .fallthrough |%s -> |%s\n", oldlayer->name, cur_layer()->name);
			continue;
		}
		// === terminating actions ===
		case Action_NoOp:
			debug("mkhd: captured\n");
			return true; // capture
		case Action_Command:
			fork_and_exec(action->argument);
			debug("mkhd: cmd: %s\n", action->argument);
			return true; // capture
		case Action_Nocapture:
			return false; // no capture
		case Action_PushLayer: {
			if (strcmp(MS_CURRENT_LAYER(mstate)->name, action->argument) == 0) {
				// by default a unbind key in a layer will .fallthrough
				// this means if you have a rule to enter layer |foo like this:
				//
				// 		|foo ctrl-a -> |bar
				//
				// pressing `ctrl-a` multiple times will trigger multiple PushLayer(|bar) actions
				// due to the key press falling through and triggering the |foo layer rule even in |bar layer.
				//
				// this can overflow the layer stack if the user spams the layer switch hotkey.
				//
				// this check guards against that by not allowing the same layer to be activated for more than once consecutively
				return true;
			}
			mstate->layerstack_cnt++;
			if (mstate->layerstack_cnt > LAYERSTACK_MAX) {
				warn("mkhd: layer stack overflow (max %d)! maybe you have a activating (->) loop in your config?\n", LAYERSTACK_MAX);
				warn("mkhd: last 5 layers: ... -> |%s -> |%s -> |%s -> |%s -> |%s", mstate->layerstack[LAYERSTACK_MAX - 5]->name,
					 mstate->layerstack[LAYERSTACK_MAX - 4]->name, mstate->layerstack[LAYERSTACK_MAX - 3]->name, mstate->layerstack[LAYERSTACK_MAX - 2]->name,
					 mstate->layerstack[LAYERSTACK_MAX - 1]->name);
				return false; // no capture
			}
			struct layer *new_layer = table_find(&mstate->layer_map, action->argument);
			MS_CURRENT_LAYER(mstate) = new_layer;
			debug("mkhd: layerswitch |> |%s\n", new_layer->name);

			return true; // capture
		}
		case Action_PopLayer:
			if (mstate->layerstack_cnt == 1) {
				warn("mkhd: can not deactivate default layer. nothing was done.");
				return true;
			}
			mstate->layerstack_cnt--;
			debug("mkhd: poplayer, back to |%s\n", MS_CURRENT_LAYER(mstate)->name);
			return true; // capture
		default:
			warn("mkhd: unknown action %d\n", action->type);
			return false;
		}
		// unreachable
		// do `continue` or `return` in the cases instead.
		error("here should be unreachable. file a bug report!\n");
	}
}

bool find_and_exec_keyevent(struct mkhd_state *mstate, struct keyevent *event, const char *process_name) {
	int fallthrough_depth = mstate->layerstack_cnt - 1;

#define cur_layer() (mstate->layerstack[fallthrough_depth])

	debug("mkhd: event: %d\n", event->key);

	struct hotkey *hotkey = table_find(&cur_layer()->hotkey_map, event);
	struct action *action = NULL;
	if (hotkey == NULL) {
		// unmatched, trigger on_unmatched event
		action = cur_layer()->on_unmatched;
	} else {
		action = find_process_action(hotkey, process_name);
	}
	return execute_action(mstate, action);
}

struct layer *create_new_layer(const char *name) {
	struct layer *layer = malloc(sizeof(struct layer));
	memset(layer, 0, sizeof(struct layer));
	layer->name = copy_string_malloc(name);

	table_init(&layer->hotkey_map, 131, (table_hash_func)hash_keyevent, (table_compare_func)compare_keyevent);

	static struct action fallthrough = {.type = Action_Fallthrough, .argument = NULL};
	static struct action noop = {.type = Action_NoOp, .argument = NULL};

	layer->on_unmatched = &fallthrough;
	layer->on_enter_layer = &noop;
	layer->on_exit_layer = &noop;

	return layer;
}

void free_layer_map(struct table *layer_map) {
	// temporarily no-op
	// todo: implement tracked malloc
}

void free_blacklist(struct table *blacklst) {
	int count;
	void **items = table_reset(blacklst, &count);
	for (int index = 0; index < count; ++index) {
		free(items[index]);
	}

	free(items);
}

void free_alias_map(struct table *alias_map) {
	int count;
	void **items = table_reset(alias_map, &count);
	for (int index = 0; index < count; ++index) {
		free(items[index]);
	}

	free(items);
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
	return (struct keyevent){.key = CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode),
							 .flags = cgevent_flags_to_hotkey_flags(CGEventGetFlags(event))};
}

bool intercept_systemkey(CGEventRef event, struct keyevent *eventkey) {
	CFDataRef event_data = CGEventCreateData(kCFAllocatorDefault, event);
	const uint8_t *data = CFDataGetBytePtr(event_data);
	uint8_t key_code = data[129];
	uint8_t key_state = data[130];
	uint8_t key_stype = data[123];
	CFRelease(event_data);

	bool result = ((key_state == NX_KEYDOWN) && (key_stype == NX_SUBTYPE_AUX_CONTROL_BUTTONS));

	if (result) {
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
