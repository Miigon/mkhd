#pragma once

#include <Carbon/Carbon.h>
#include <stdbool.h>
#include <stdint.h>

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
	Action_NoOp = 0,  // do nothing but capture the key event
	Action_Command,	  // run a command (capture the key event)
	Action_Nocapture, // do nothing and keep the original OS behaviour of the key event

	// layer stack manipulation actions
	Action_PushLayer,		 // activate a new layer (add it to the layer stack)
	Action_PushLayerOneshot, // activate a new layer for a single key press
	Action_PopLayer,		 // pop the top layer from the layer stack

	Action_Fallthrough, // delegate the event to the next lower layer in the layer stack
						// (default behaviour of unmatched keys in any layers.)
						// once an event falls through the lowest layer, it behaves like a
						// Nocapture and registers as a regular key press.
};

struct action {
	enum action_type type;
	const char *argument;
};

enum keyevent_type {
	Event_Null,

	Event_Key,

	// pseudo keys
	Event_KeyDown,
	Event_KeyUp,

	Event_Unmatched, // triggers when a key matches no hotkey in this layer (default: Fallthrough)
	Event_EnterLayer,
	Event_ExitLayer,
};

struct keyevent {
	enum keyevent_type type;
	uint32_t flags;
	uint32_t key;
};

struct hotkey {
	struct keyevent event;

	char **process_names;
	struct action **actions;

	struct action *process_default_action;
};

struct layer {
	const char *name;
	struct table hotkey_map; // <keyevent, hotkey>
};

struct layerstack_frame {
	struct layer *l;
	bool oneshot;
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

struct mkhd_state;

// returns whether to capture the event or not
bool find_and_exec_keyevent(struct mkhd_state *mstate, struct keyevent *event, const char *process_name);

struct layer *create_new_layer(const char *name_moved);
void add_hotkey_to_layer(struct layer *layer, struct hotkey *hotkey);

void init_shell(void);
