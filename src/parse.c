#include "parse.h"

#include <IOKit/hidsystem/ev_keymap.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hashtable.h"
#include "hotkey.h"
#include "locale.h"
#include "log.h"
#include "mkhd.h"
#include "sbuffer.h"
#include "tokenize.h"
#include "utils.h"

#define DEFVAR_FROM_TOKEN_TEXT(var, token)                                                                             \
	char(var)[(token).length + 1];                                                                                     \
	copy_string_count_nomalloc((var), (token).text, (token).length);

static struct layer *find_layer_or_create(struct parser *parser, const char *name) {
	struct layer *layer = table_find(parser->layer_map, name);

	if (layer != NULL) {
		return layer;
	}

	// layer not found, implicitly create them.
	layer = create_new_layer(name);
	table_add(parser->layer_map, layer->name, layer);

	return layer;
}

static char *read_file(const char *file) {
	unsigned length;
	char *buffer = NULL;
	FILE *handle = fopen(file, "r");

	if (handle) {
		fseek(handle, 0, SEEK_END);
		length = ftell(handle);
		fseek(handle, 0, SEEK_SET);
		buffer = tr_malloc(length + 1);
		fread(buffer, length, 1, handle);
		buffer[length] = '\0';
		fclose(handle);
	}

	return buffer;
}

static bool parser_match_action(struct parser *parser) {
	return parser_match(parser, Token_Command) || parser_match(parser, Token_Option);
}

static struct action *parse_action(struct parser *parser) {
	struct token token = parser_previous(parser);
	struct action *action = tr_malloc(sizeof(struct action));
	action->type = Action_NoOp;

	debug("\taction: ");

	if (token.type == Token_Command) {
		action->type = Action_Command;
		action->argument = copy_string_count_malloc(token.text, token.length);
		debug("[cmd]: '%s'\n", action->argument);
	} else if (token.type == Token_Option) {
		DEFVAR_FROM_TOKEN_TEXT(option, token);
		bool activate_oneshot = strcmp(option, "oneshot") == 0;
		if (activate_oneshot || strcmp(option, "activate") == 0) {
			// activate a new layer
			if (parser_match(parser, Token_Layer)) {
				struct token layer_token = parser_previous(parser);
				if (layer_token.length == 0) {
					parser_report_error(parser, layer_token, "layer name can not be empty\n");
					return action;
				}
				action->type = activate_oneshot ? Action_PushLayerOneshot : Action_PushLayer;
				action->argument = copy_string_count_malloc(layer_token.text, layer_token.length);
				debug("[activate]|%s\n", action->argument);
			} else {
				parser_report_error(parser, parser_peek(parser), "expected layer\n");
			}
		} else if (strcmp(option, "deactivate") == 0) {
			// pops current layer from the layer stack
			action->type = Action_PopLayer;
			debug("[layerswitch]!default\n");
		} else if (strcmp(option, "fallthrough") == 0) {
			// passthrough to the layer below in the layer stack
			action->type = Action_Fallthrough;
			debug("[pass]|.fallthrough\n");
		} else if (strcmp(option, "nop") == 0) {
			// capture the key press and do nothing
			action->type = Action_NoOp; // default behaviour is capture already, so do nothing
			debug("[nop]\n");
		} else if (strcmp(option, "nocapture") == 0) {
			// do nothing and do not capture the key press
			// let system do it's thing with it like normal
			action->type = Action_Nocapture;
			debug("[nocapture]\n");
		} else {
			parser_report_error(parser, token, "invalid option as action: .%s\n", option);
		}
	}
	return action;
}

static void parse_process_action_mappings(struct parser *parser, struct hotkey *hotkey) {
	bool first_iter = true;
	do {
		if (parser_match(parser, Token_String)) {
			struct token name_token = parser_previous(parser);
			char *name = copy_string_count_malloc(name_token.text, name_token.length);
			for (char *s = name; *s; ++s)
				*s = tolower(*s);
			buf_push(hotkey->process_names, name);
			if (parser_match_action(parser)) {
				buf_push(hotkey->actions, parse_action(parser));
			} else {
				parser_report_error(parser, parser_peek(parser), "expected action\n");
				return;
			}
		} else if (parser_match(parser, Token_Wildcard)) {
			if (parser_match_action(parser)) {
				hotkey->process_default_action = parse_action(parser);
			} else {
				parser_report_error(parser, parser_peek(parser), "expected action\n");
				return;
			}
		} else if (parser_match(parser, Token_EndList)) {
			if (first_iter) {
				parser_report_error(parser, parser_peek(parser), "required at least one process-action mapping\n");
			}
			return;
		} else {
			parser_report_error(parser, parser_peek(parser), "process-action mapping expected\n");
			return;
		}
		first_iter = false;
	} while (true);
}

static uint32_t keycode_from_hex(char *hex) {
	uint32_t result;
	sscanf(hex, "%x", &result);
	return result;
}

static uint32_t parse_key_hex(struct parser *parser) {
	struct token key = parser_previous(parser);
	DEFVAR_FROM_TOKEN_TEXT(hex, key);
	uint32_t keycode = keycode_from_hex(hex);
	debug("\tkey: '%.*s' (0x%02x)\n", key.length, key.text, keycode);
	return keycode;
}

static uint32_t parse_key(struct parser *parser) {
	uint32_t keycode;
	struct token key = parser_previous(parser);
	keycode = keycode_from_char(*key.text);
	debug("\tkey: '%c' (0x%02x)\n", *key.text, keycode);
	return keycode;
}

#define KEY_HAS_IMPLICIT_FN_MOD 4
#define KEY_HAS_IMPLICIT_NX_MOD 35
static uint32_t literal_keycode_value[] = {
	kVK_Return,
	kVK_Tab,
	kVK_Space,
	kVK_Delete,
	kVK_Escape,
	kVK_ForwardDelete,
	kVK_Home,
	kVK_End,
	kVK_PageUp,
	kVK_PageDown,
	kVK_Help,
	kVK_LeftArrow,
	kVK_RightArrow,
	kVK_UpArrow,
	kVK_DownArrow,
	kVK_F1,
	kVK_F2,
	kVK_F3,
	kVK_F4,
	kVK_F5,
	kVK_F6,
	kVK_F7,
	kVK_F8,
	kVK_F9,
	kVK_F10,
	kVK_F11,
	kVK_F12,
	kVK_F13,
	kVK_F14,
	kVK_F15,
	kVK_F16,
	kVK_F17,
	kVK_F18,
	kVK_F19,
	kVK_F20,

	NX_KEYTYPE_SOUND_UP,
	NX_KEYTYPE_SOUND_DOWN,
	NX_KEYTYPE_MUTE,
	NX_KEYTYPE_PLAY,
	NX_KEYTYPE_PREVIOUS,
	NX_KEYTYPE_NEXT,
	NX_KEYTYPE_REWIND,
	NX_KEYTYPE_FAST,
	NX_KEYTYPE_BRIGHTNESS_UP,
	NX_KEYTYPE_BRIGHTNESS_DOWN,
	NX_KEYTYPE_ILLUMINATION_UP,
	NX_KEYTYPE_ILLUMINATION_DOWN,
	NX_KEYTYPE_CAPS_LOCK,
};

static inline void handle_implicit_literal_flags(struct keyevent *event, int literal_index) {
	if ((literal_index > KEY_HAS_IMPLICIT_FN_MOD) && (literal_index < KEY_HAS_IMPLICIT_NX_MOD)) {
		event->flags |= Hotkey_Flag_Fn;
	} else if (literal_index >= KEY_HAS_IMPLICIT_NX_MOD) {
		event->flags |= Hotkey_Flag_NX;
	}
}

static void parse_key_literal(struct parser *parser, struct keyevent *keyevent) {
	struct token key = parser_previous(parser);
	for (int i = 0; i < array_count(literal_keycode_str); ++i) {
		if (token_equals(key, literal_keycode_str[i])) {
			handle_implicit_literal_flags(keyevent, i);
			keyevent->key = literal_keycode_value[i];
			debug("\tkey: '%.*s' (0x%02x)\n", key.length, key.text, keyevent->key);
			break;
		}
	}
}

static enum hotkey_flag modifier_flags_value[] = {
	Hotkey_Flag_Alt,	Hotkey_Flag_LAlt,	 Hotkey_Flag_RAlt,	   Hotkey_Flag_Shift,
	Hotkey_Flag_LShift, Hotkey_Flag_RShift,	 Hotkey_Flag_Cmd,	   Hotkey_Flag_LCmd,
	Hotkey_Flag_RCmd,	Hotkey_Flag_Control, Hotkey_Flag_LControl, Hotkey_Flag_RControl,
	Hotkey_Flag_Fn,		Hotkey_Flag_Hyper,	 Hotkey_Flag_Meh,	   Hotkey_Flag_NX,
};

#define INVALID_KEY UINT32_MAX

static void expand_alias(struct parser *parser, struct keyevent *dst, struct token alias, bool *contains_mod) {
	if (parser->alias_map == NULL) {
		// parser is in text layer (eg. `synthesize_key()`)
		parser_report_error(parser, alias, "aliases not supported in this layer\n");
		return;
	}
	DEFVAR_FROM_TOKEN_TEXT(alias_name, alias);

	debug("\tuse_alias: $%s\n", alias_name);

	struct keyevent *alias_keyevent = table_find(parser->alias_map, alias_name);
	if (alias_keyevent == NULL) {
		parser_report_error(parser, alias, "undefined alias $%s\n", alias_name);
		return;
	}
	if (alias_keyevent->key != INVALID_KEY) {
		// alias contains keycode
		if (dst->key != INVALID_KEY) {
			parser_report_error(parser, alias, "multiple keycodes specified by using alias $%s\n", alias_name);
			return;
		}
		dst->key = alias_keyevent->key;
	}
	dst->flags |= alias_keyevent->flags;
	if (contains_mod)
		*contains_mod = (alias_keyevent->flags & Hotkey_Flag_Modifier) != 0;
}

static bool parse_modifier(struct parser *parser, struct keyevent *keyevent) {
	bool first_iter = true;
	do {
		if (parser_match(parser, Token_Modifier)) {
			struct token modifier = parser_previous(parser);

			for (int i = 0; i < array_count(modifier_flags_str); ++i) {
				if (token_equals(modifier, modifier_flags_str[i])) {
					keyevent->flags |= modifier_flags_value[i];
					debug("\tmod: '%s'\n", modifier_flags_str[i]);
					break;
				}
			}
		} else if (parser_match(parser, Token_Alias)) {
			// starts with an alias that might contain a modifier
			struct token alias = parser_previous(parser);
			bool contains_mod = false;
			expand_alias(parser, keyevent, alias, &contains_mod);
			if (parser->error) {
				return false;
			}

			if (!contains_mod && !first_iter) {
				parser_report_error(parser, alias, "alias $%.*s does not contain any modifiers\n", alias.length,
									alias.text);
				return false;
			}
		} else {
			if (!first_iter)
				parser_report_error(parser, parser_advance(parser), "expected modifier\n");
			return false;
		}

		first_iter = false;
	} while (parser_match(parser, Token_Plus));
	return true;
}

static int parse_layers(struct parser *parser, struct layer **layer_list, int max_layers) {
	int idx = 0;
	if (!parser_check(parser, Token_Layer)) {
		// no layer specified, go with default layer.
		struct layer *layer = find_layer_or_create(parser, DEFAULT_LAYER);
		debug("\tlayer: '%s'\n", layer->name);
		layer_list[0] = layer;
		return 1;
	}
	// layer(s) specified.
	do {
		if (!parser_match(parser, Token_Layer)) {
			parser_report_error(parser, parser_peek(parser), "layer expected\n");
			return -1;
		}
		struct token token = parser_previous(parser);
		if (idx >= max_layers) {
			parser_report_error(parser, token, "too many layer specifiers for one single rule (max %d)\n", max_layers);
			return -1;
		}

		DEFVAR_FROM_TOKEN_TEXT(name, token);
		struct layer *layer = find_layer_or_create(parser, name);

		debug("\tlayer: '%s'\n", layer->name);
		layer_list[idx++] = layer;
	} while (parser_match(parser, Token_Comma));
	return idx;
}

static void parse_hotkey(struct parser *parser) {
	struct hotkey *hotkey = tr_malloc(sizeof(struct hotkey));
	memset(hotkey, 0, sizeof(struct hotkey));

	debug("hotkey :: #%d {\n", parser->current_token.line);

	struct layer *layer_list[256];
	int layer_cnt = parse_layers(parser, layer_list, array_count(layer_list));
	if (parser->error)
		return;

	parse_keyevent(parser, &hotkey->event, false);
	if (parser->error)
		return;

	if (parser_match_action(parser)) {
		hotkey->process_default_action = parse_action(parser);
	} else if (parser_match(parser, Token_BeginList)) {
		parse_process_action_mappings(parser, hotkey);
	} else {
		parser_report_error(parser, parser_peek(parser), "expected action\n");
	}
	if (parser->error)
		return;

	// add hotkey to its layer(s)
	// must do it after `parse_keyevent()`
	for (int i = 0; i < layer_cnt; i++) {
		struct layer *layer = layer_list[i];
		add_hotkey_to_layer(layer, hotkey);
	}

	debug("}\n");
}

void parse_option_blocklist(struct parser *parser) {
	if (parser_match(parser, Token_String)) {
		struct token name_token = parser_previous(parser);
		char *name = copy_string_count_malloc(name_token.text, name_token.length);
		for (char *s = name; *s; ++s)
			*s = tolower(*s);
		debug("\t%s\n", name);
		table_add(parser->blocklst, name, name);
		parse_option_blocklist(parser);
	} else if (parser_match(parser, Token_EndList)) {
		if (parser->blocklst->count == 0) {
			parser_report_error(parser, parser_previous(parser), "list must contain at least one value\n");
		}
	} else {
		parser_report_error(parser, parser_peek(parser), "expected process name or ']'\n");
	}
}

void parse_option_load(struct parser *parser, struct token option) {
	struct token filename_token = parser_previous(parser);
	char *filename = copy_string_count_malloc(filename_token.text, filename_token.length);
	debug("\t%s\n", filename);

	if (*filename != '/') {
		char *directory = file_directory(parser->file);

		size_t directory_length = strlen(directory);
		size_t filename_length = strlen(filename);
		size_t total_length = directory_length + filename_length + 2;

		char *absolutepath = tr_malloc(total_length * sizeof(char));
		snprintf(absolutepath, total_length, "%s/%s", directory, filename);
		tr_free(filename);

		filename = absolutepath;
	}

	buf_push(parser->load_directives, ((struct load_directive){.file = filename, .option = option}));
}

void parse_option_alias(struct parser *parser) {
	struct token alias_token = parser_previous(parser);
	char *alias_name = copy_string_count_malloc(alias_token.text, alias_token.length);
	debug("\talias_name: $%s\n", alias_name);

	struct keyevent *keyevent = tr_malloc(sizeof(struct hotkey));
	memset(keyevent, 0, sizeof(struct hotkey));
	parse_keyevent(parser, keyevent, true);

	// later definition of the same alias takes predecence over previous ones.
	table_replace(parser->alias_map, alias_name, keyevent);
}

void parse_option(struct parser *parser) {
	parser_match(parser, Token_Option);
	struct token option = parser_previous(parser);
	if (token_equals(option, "blocklist")) {
		if (parser_match(parser, Token_BeginList)) {
			debug("blocklist :: #%d {\n", option.line);
			parse_option_blocklist(parser);
			debug("}\n");
		} else {
			parser_report_error(parser, option, "expected '[' followed by list of process names\n");
		}
	} else if (token_equals(option, "load")) {
		if (parser_match(parser, Token_String)) {
			debug("load :: #%d {\n", option.line);
			parse_option_load(parser, option);
			debug("}\n");
		} else {
			parser_report_error(parser, option, "expected filename\n");
		}
	} else if (token_equals(option, "alias")) {
		if (parser_match(parser, Token_Alias)) {
			debug("alias :: #%d {\n", option.line);
			parse_option_alias(parser);
			debug("}\n");
		} else {
			parser_report_error(parser, option, "expected $alias_name\n");
		}
	} else {
		parser_report_error(parser, option, "invalid option specified\n");
	}
}

bool parse_config(struct parser *parser) {
	while (!parser_eof(parser)) {
		if (parser->error)
			break;

		if (parser_check(parser, Token_Identifier) || parser_check(parser, Token_Modifier) ||
			parser_check(parser, Token_Literal) || parser_check(parser, Token_Key_Hex) ||
			parser_check(parser, Token_Key) || parser_check(parser, Token_Alias) || parser_check(parser, Token_Layer)) {
			parse_hotkey(parser);
		} else if (parser_check(parser, Token_Option)) {
			parse_option(parser);
		} else {
			parser_report_error(parser, parser_peek(parser), "expected decl\n");
		}
	}

	return !parser->error;
}

static bool parse_keycombination(struct parser *parser, struct keyevent *keyevent, bool allow_no_keycode) {
	bool found_modifier = parse_modifier(parser, keyevent);
	if (parser->error) {
		return false;
	}

	if (keyevent->key != INVALID_KEY) {
		// found keycode within one of the aliases while parsing modifier.
		// no need to look any further for keycode.
		return true;
	}

	if (found_modifier) {
		if (!parser_check(parser, Token_Dash)) {
			if (allow_no_keycode) {
				return true;
			}
			parser_report_error(parser, parser_peek(parser), "expected '-'\n");
			return false;
		}
		parser_advance(parser);
	}

	if (parser_match(parser, Token_Key)) {
		keyevent->key = parse_key(parser);
	} else if (parser_match(parser, Token_Key_Hex)) {
		keyevent->key = parse_key_hex(parser);
	} else if (parser_match(parser, Token_Literal)) {
		parse_key_literal(parser, keyevent);
	} else if (parser_match(parser, Token_Alias)) {
		struct token alias = parser_previous(parser);
		expand_alias(parser, keyevent, alias, NULL);
	} else {
		parser_report_error(parser, parser_peek(parser), "expected key-literal\n");
		return false;
	}

	return true;
}

bool parse_keyevent(struct parser *parser, struct keyevent *keyevent, bool allow_no_keycode) {
	if (!(parser_check(parser, Token_Modifier) || parser_check(parser, Token_Literal) ||
		  parser_check(parser, Token_Key_Hex) || parser_check(parser, Token_Key) || parser_check(parser, Token_Alias) ||
		  parser_check(parser, Token_Event))) {
		parser_report_error(parser, parser_peek(parser), "expected a hotkey\n");
		return false;
	}

	keyevent->key = INVALID_KEY; // keycode 0 is actually a valid keycode (kVK_ANSI_A)

	if (parser_match(parser, Token_Event)) { // @pseudo_keys
		DEFVAR_FROM_TOKEN_TEXT(pktype, parser_previous(parser));
		if (strcmp(pktype, "unmatched") == 0) {
			keyevent->type = Event_Unmatched;
		} else if (strcmp(pktype, "enter_layer") == 0) {
			keyevent->type = Event_EnterLayer;
		} else if (strcmp(pktype, "exit_layer") == 0) {
			keyevent->type = Event_ExitLayer;
		} else if (strcmp(pktype, "keydown") == 0) {
			keyevent->type = Event_KeyDown;
		} else if (strcmp(pktype, "keyup") == 0) {
			keyevent->type = Event_KeyUp;
		} else {
			parser_report_error(parser, parser_previous(parser), "invalid pseudo key: @%s\n", pktype);
			return false;
		}
		debug("\tpseudo_key: @%s\n", pktype);

		if (keyevent->type == Event_Key || keyevent->type == Event_KeyDown || keyevent->type == Event_KeyUp) {
			if (!parser_match(parser, Token_BracketLeft)) {
				parser_report_error(parser, parser_peek(parser), "expected (\n");
				return false;
			}
			bool ret = parse_keycombination(parser, keyevent, true);
			if (!parser_match(parser, Token_BracketRight)) {
				parser_report_error(parser, parser_peek(parser), "expected )\n");
				return false;
			}
			return ret;
		}
		return true;
	} else {
		keyevent->type = Event_Key;
		return parse_keycombination(parser, keyevent, allow_no_keycode);
	}
}

struct token parser_peek(struct parser *parser) { return parser->current_token; }

struct token parser_previous(struct parser *parser) { return parser->previous_token; }

bool parser_eof(struct parser *parser) {
	struct token token = parser_peek(parser);
	return token.type == Token_EndOfStream;
}

struct token parser_advance(struct parser *parser) {
	if (!parser_eof(parser)) {
		parser->previous_token = parser->current_token;
		parser->current_token = get_token(&parser->tokenizer);
	}
	return parser_previous(parser);
}

bool parser_check(struct parser *parser, enum token_type type) {
	if (parser_eof(parser))
		return false;
	struct token token = parser_peek(parser);
	return token.type == type;
}

bool parser_match(struct parser *parser, enum token_type type) {
	if (parser_check(parser, type)) {
		parser_advance(parser);
		return true;
	}
	return false;
}

void parser_report_error(struct parser *parser, struct token token, const char *format, ...) {
	va_list args;
	va_start(args, format);
	fprintf(stderr, "#%d:%d ", token.line, token.cursor);
	vfprintf(stderr, format, args);
	va_end(args);
	parser->error = true;
}

void parser_do_directives(struct parser *parser, struct hotloader *hotloader, bool thwart_hotloader) {
	for (int i = 0; i < buf_len(parser->load_directives); ++i) {
		struct load_directive load = parser->load_directives[i];

		struct parser directive_parser;
		if (parser_init(&directive_parser, parser->layer_map, parser->blocklst, parser->alias_map, load.file)) {
			if (!thwart_hotloader) {
				hotloader_add_file(hotloader, load.file);
			}

			if (parse_config(&directive_parser)) {
				parser_do_directives(&directive_parser, hotloader, thwart_hotloader);
			}

			parser_destroy(&directive_parser);
		} else {
			warn("mkhd: could not open file '%s' from load directive #%d:%d\n", load.file, load.option.line,
				 load.option.cursor);
		}

		tr_free(load.file);
	}
	buf_free(parser->load_directives);
}

bool parser_init(struct parser *parser, struct table *layer_map, struct table *blocklst, struct table *alias_map,
				 char *file) {
	memset(parser, 0, sizeof(struct parser));
	char *buffer = read_file(file);
	if (buffer) {
		parser->file = file;
		parser->layer_map = layer_map;
		parser->blocklst = blocklst;
		parser->alias_map = alias_map;
		tokenizer_init(&parser->tokenizer, buffer);
		parser_advance(parser);
		return true;
	}
	return false;
}

bool parser_init_text(struct parser *parser, char *text) {
	memset(parser, 0, sizeof(struct parser));
	tokenizer_init(&parser->tokenizer, text);
	parser_advance(parser);
	return true;
}

void parser_destroy(struct parser *parser) { tr_free(parser->tokenizer.buffer); }
