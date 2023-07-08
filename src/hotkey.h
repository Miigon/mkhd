#pragma once

#include <Carbon/Carbon.h>
#include <stdbool.h>
#include <stdint.h>

#include "mkhd.h"

#define Modifier_Keycode_Alt 0x3A
#define Modifier_Keycode_Shift 0x38
#define Modifier_Keycode_Cmd 0x37
#define Modifier_Keycode_Ctrl 0x3B
#define Modifier_Keycode_Fn 0x3F

enum osx_event_mask {
	Event_Mask_Alt = 0x00080000,
	Event_Mask_LAlt = 0x00000020,
	Event_Mask_RAlt = 0x00000040,
	Event_Mask_Shift = 0x00020000,
	Event_Mask_LShift = 0x00000002,
	Event_Mask_RShift = 0x00000004,
	Event_Mask_Cmd = 0x00100000,
	Event_Mask_LCmd = 0x00000008,
	Event_Mask_RCmd = 0x00000010,
	Event_Mask_Control = 0x00040000,
	Event_Mask_LControl = 0x00000001,
	Event_Mask_RControl = 0x00002000,
	Event_Mask_Fn = kCGEventFlagMaskSecondaryFn,
};

enum hotkey_flag {
	Hotkey_Flag_Alt = (1 << 0),
	Hotkey_Flag_LAlt = (1 << 1),
	Hotkey_Flag_RAlt = (1 << 2),
	Hotkey_Flag_Shift = (1 << 3),
	Hotkey_Flag_LShift = (1 << 4),
	Hotkey_Flag_RShift = (1 << 5),
	Hotkey_Flag_Cmd = (1 << 6),
	Hotkey_Flag_LCmd = (1 << 7),
	Hotkey_Flag_RCmd = (1 << 8),
	Hotkey_Flag_Control = (1 << 9),
	Hotkey_Flag_LControl = (1 << 10),
	Hotkey_Flag_RControl = (1 << 11),
	Hotkey_Flag_Fn = (1 << 12),
	Hotkey_Flag_Modifier = ((Hotkey_Flag_Fn << 1) - 1),

	Hotkey_Flag_NX = (1 << 15),
	// TODO: deprecate these
	Hotkey_Flag_Hyper = (Hotkey_Flag_Cmd | Hotkey_Flag_Alt | Hotkey_Flag_Shift | Hotkey_Flag_Control),
	Hotkey_Flag_Meh = (Hotkey_Flag_Control | Hotkey_Flag_Shift | Hotkey_Flag_Alt)
};

#include "hashtable.h"

struct carbon_event;

enum action_type {
	Action_NoOp = 0,
	Action_Command,
	Action_ModeSwitch,
	Action_PopMode, // pop back to the mode before entering this mode

	Action_PassMode,	  // pass keyevent to another mode
	Action_Fallthrough,	  // pass to the next lower mode in the mode stack
	Action_PassNocapture, // pass to outside mkhd (no capture, work as regular key press)
};

struct action {
	enum action_type type;
	const char *argument;
};

struct keyevent {
	// todo: implement special event (@enter_mode, @exit_mode, @unmatched, etc)
	uint32_t flags;
	uint32_t key;
};

struct hotkey {
	struct keyevent event;

	char **process_names;
	struct action **actions;

	struct action *process_default_action;
};

struct mode {
	const char *name;
	struct table hotkey_map; // <keyevent, hotkey>

	// actions triggered for special events (@unmatched, @enter_mode, etc).
	// no need to null-check these. they are guaranteed to be initialized. (most of them are NoOp)
	struct action *on_unmatched; // what to do when a key matches no hotkey in this mode (default: Fallthrough)
	struct action *on_enter_mode;
	struct action *on_exit_mode;
};

static inline void add_flags(struct keyevent *event, uint32_t flag) { event->flags |= flag; }
static inline bool has_flags(struct keyevent *event, uint32_t flag) { return event->flags & flag; }
static inline void clear_flags(struct keyevent *event, uint32_t flag) { event->flags &= ~flag; }

bool compare_string(char *a, char *b);
unsigned long hash_string(char *key);

bool compare_keyevent(struct keyevent *a, struct keyevent *b);
unsigned long hash_keyevent(struct keyevent *a);

struct keyevent create_keyevent_from_CGEvent(CGEventRef event);
bool intercept_systemkey(CGEventRef event, struct keyevent *eventkey);

// returns whether to capture the event or not
bool find_and_exec_keyevent(struct mkhd_state *mstate, struct keyevent *event, const char *process_name);

struct mode *create_new_mode(const char *name_moved);

void free_mode_map(struct table *mode_map);
void free_blacklist(struct table *blacklst);
void free_alias_map(struct table *alias_map);

void init_shell(void);
