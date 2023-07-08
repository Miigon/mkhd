#include "hotkey.h"

#include "carbon.h"
#include "log.h"
#include "sbuffer.h"
#include "utils.h"

#define HOTKEY_FOUND ((1) << 0)
#define MODE_CAPTURE(a) ((a) << 1)
#define HOTKEY_PASSTHROUGH(a) ((a) << 2)

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

	for (int i = 0; i < buf_len(hotkey->process_names); ++i) {
		if (same_string(process_name, hotkey->process_names[i])) {
			result = hotkey->actions[i];
			found = true;
			break;
		}
	}

	if (!found)
		result = hotkey->process_default_action;

	return result;
}

bool find_and_exec_keyevent(struct mkhd_state *mstate, struct keyevent *event, const char *process_name) {
	struct mode *in_mode = MS_CURRENT_MODE(mstate);

	// do action
	while (true) {
		struct hotkey *hotkey = table_find(&in_mode->hotkey_map, event);
		struct action *action = find_process_action(hotkey, process_name);
		switch (action->type) {
		// === non-terminating actions ===
		case Action_PassMode:
			MS_CURRENT_MODE(mstate) = table_find(&mstate->mode_map, action->argument);
			continue;
		case Action_Fallthrough: {
			if (mstate->modestack_cnt == 1) {
				// special case: `-> |.fallthrough` at the lowest mode frame is the same as `-> |.nocapture`
				return false;
			}
			mstate->modestack_cnt--;
			in_mode = mstate->modestack[mstate->modestack_cnt - 1];
		}
		// === terminating actions ===
		case Action_NoOp:
			return true; // capture
		case Action_Command:
			fork_and_exec(action->argument);
			return true; // capture
		case Action_ModeSwitch: {
			struct mode *new_mode = table_find(&mstate->mode_map, action->argument);
			mstate->modestack[mstate->modestack_cnt++] = new_mode;

			if (mstate->modestack_cnt >= MODESTACK_MAX) {
				warn("mode stack overflow! (max %d) maybe you have a passing (->) loop in your config?\n", MODESTACK_MAX - 1);
				warn("last 5 mode stack frame: ... -> |%s -> |%s -> |%s -> |%s -> |%s", mstate->modestack[MODESTACK_MAX - 5]->name,
					 mstate->modestack[MODESTACK_MAX - 4]->name, mstate->modestack[MODESTACK_MAX - 3]->name, mstate->modestack[MODESTACK_MAX - 2]->name,
					 mstate->modestack[MODESTACK_MAX - 1]->name);
				return false; // no capture
			}
			return true; // capture
		}
		case Action_PopMode: {
			mstate->modestack_cnt--;
			return true; // capture
		case Action_PassNocapture:
			return false; // no capture
		}
		}
		// unreachable
	}
}

struct mode *create_new_mode(const char *name) {
	struct mode *mode = malloc(sizeof(struct mode));
	memset(mode, 0, sizeof(struct mode));
	mode->name = copy_string_malloc(name);

	table_init(&mode->hotkey_map, 131, (table_hash_func)hash_keyevent, (table_compare_func)compare_keyevent);

	static struct action fallthrough = {.type = Action_Fallthrough, .argument = NULL};
	static struct action noop = {.type = Action_NoOp, .argument = NULL};

	mode->on_unmatched = &fallthrough;
	mode->on_enter_mode = &noop;
	mode->on_exit_mode = &noop;

	return mode;
}

void free_mode_map(struct table *mode_map) {
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
