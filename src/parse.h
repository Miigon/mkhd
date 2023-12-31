#pragma once

#include <stdbool.h>

#include "hotkey.h"
#include "hotload.h"
#include "tokenize.h"

struct load_directive {
	char *file;
	struct token option;
};

struct table;
struct parser {
	char *file;
	struct token previous_token;
	struct token current_token;
	struct tokenizer tokenizer;
	struct table *layer_map;
	struct table *blocklst;
	struct table *alias_map;
	struct load_directive *load_directives;
	bool error;
};

bool parse_config(struct parser *parser);
bool parse_keyevent(struct parser *parser, struct keyevent *hotkey, bool allow_no_keycode);
struct keyevent *parse_keyevent_list(struct parser *parser);

struct token parser_peek(struct parser *parser);
struct token parser_previous(struct parser *parser);
bool parser_eof(struct parser *parser);
struct token parser_advance(struct parser *parser);
bool parser_check(struct parser *parser, enum token_type type);
bool parser_match(struct parser *parser, enum token_type type);
bool parser_init(struct parser *parser, struct table *layer_map, struct table *blocklst, struct table *alias_map,
				 char *file);
bool parser_init_text(struct parser *parser, char *text);
void parser_destroy(struct parser *parser);
void parser_report_error(struct parser *parser, struct token token, const char *format, ...);
void parser_do_directives(struct parser *parser, struct hotloader *hotloader, bool thwart_hotloader);
