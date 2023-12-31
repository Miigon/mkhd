#include "tokenize.h"

#include <ctype.h>
#include <stdbool.h>

#define array_count(a) (sizeof((a)) / sizeof(*(a)))

int token_equals(struct token token, const char *match) {
	const char *at = match;
	for (int i = 0; i < token.length; ++i, ++at) {
		if ((*at == 0) || (token.text[i] != *at)) {
			return false;
		}
	}
	return (*at == 0);
}

static void advance(struct tokenizer *tokenizer) {
	if (*tokenizer->at == '\n') {
		tokenizer->cursor = 0;
		++tokenizer->line;
	}
	++tokenizer->cursor;
	++tokenizer->at;
}

static void eat_whitespace(struct tokenizer *tokenizer) {
	while (*tokenizer->at && isspace(*tokenizer->at)) {
		advance(tokenizer);
	}
}

static void eat_comment(struct tokenizer *tokenizer) {
	while (*tokenizer->at && *tokenizer->at != '\n') {
		advance(tokenizer);
	}
}

static void eat_command(struct tokenizer *tokenizer) {
	while (*tokenizer->at && *tokenizer->at != '\n') {
		if (*tokenizer->at == '\\') {
			advance(tokenizer);
		}
		advance(tokenizer);
	}
}

static void eat_hex(struct tokenizer *tokenizer) {
	while ((*tokenizer->at) && ((isdigit(*tokenizer->at)) || (*tokenizer->at >= 'A' && *tokenizer->at <= 'F'))) {
		advance(tokenizer);
	}
}

static void eat_string(struct tokenizer *tokenizer) {
	/*
	 * NOTE(koekeishiya): This is NOT proper string parsing code, as we do
	 * not check for escaped '"' here. At the time of writing, this is only
	 * supposed to be used for parsing names of processes, and such names
	 * should not contain escaped quotes at all. We are lazy and simply do
	 * the most basic implementation that fulfills our current requirement.
	 */

	while (*tokenizer->at && *tokenizer->at != '"') {
		advance(tokenizer);
	}
}

static inline bool isidentifier_start(char c) { return isalpha(c) || c == '_'; }

static void eat_identifier(struct tokenizer *tokenizer) {
	if (isidentifier_start(*tokenizer->at)) {
		advance(tokenizer);
	}

	while (isdigit(*tokenizer->at) || isidentifier_start(*tokenizer->at)) {
		advance(tokenizer);
	}
}

static enum token_type resolve_identifier_type(struct token token) {
	if (token.length == 1) {
		return Token_Key;
	}

	for (int i = 0; i < array_count(modifier_flags_str); ++i) {
		if (token_equals(token, modifier_flags_str[i])) {
			return Token_Modifier;
		}
	}

	for (int i = 0; i < array_count(literal_keycode_str); ++i) {
		if (token_equals(token, literal_keycode_str[i])) {
			return Token_Literal;
		}
	}

	return Token_Identifier;
}

static void do_special_identifier(struct tokenizer *tokenizer, struct token *t, enum token_type type) {
	t->text = tokenizer->at;
	t->line = tokenizer->line;
	t->cursor = tokenizer->cursor;

	eat_identifier(tokenizer);
	t->length = tokenizer->at - t->text;
	t->type = type;
}

struct token peek_token(struct tokenizer *tokenizer) { return get_token(tokenizer); }

struct token get_token(struct tokenizer *tokenizer) {
	struct token token;
	char c;

	eat_whitespace(tokenizer);

	token.length = 1;
	token.text = tokenizer->at;
	token.line = tokenizer->line;
	token.cursor = tokenizer->cursor;
	c = *token.text;
	advance(tokenizer);

	switch (c) {
	case '\0':
		token.type = Token_EndOfStream;
		break;
	case '+': {
		token.type = Token_Plus;
	} break;
	case ',': {
		token.type = Token_Comma;
	} break;
	case '<': {
		token.type = Token_Insert;
	} break;
	case '*': {
		token.type = Token_Wildcard;
	} break;
	case '[': {
		token.type = Token_BeginList;
	} break;
	case ']': {
		token.type = Token_EndList;
	} break;
	case '(': {
		token.type = Token_BracketLeft;
	} break;
	case ')': {
		token.type = Token_BracketRight;
	} break;
	case '"': {
		token.text = tokenizer->at;
		token.line = tokenizer->line;
		token.cursor = tokenizer->cursor;

		eat_string(tokenizer);
		token.length = tokenizer->at - token.text;
		token.type = Token_String;

		advance(tokenizer);
	} break;
	case '#': {
		eat_comment(tokenizer);
		token = get_token(tokenizer);
	} break;
	case '-': {
		if (*tokenizer->at == '>') {
			advance(tokenizer);
			token.length = tokenizer->at - token.text;
			token.type = Token_Arrow;
		} else {
			token.type = Token_Dash;
		}
	} break;
	case ':': {
		eat_whitespace(tokenizer);

		token.text = tokenizer->at;
		token.line = tokenizer->line;
		token.cursor = tokenizer->cursor;

		eat_command(tokenizer);
		token.length = tokenizer->at - token.text;
		token.type = Token_Command;
	} break;
	// special identifiers
	case '.': {
		do_special_identifier(tokenizer, &token, Token_Option);
	} break;
	case '$': {
		do_special_identifier(tokenizer, &token, Token_Alias);
	} break;
	case '@': {
		do_special_identifier(tokenizer, &token, Token_Event);
	} break;
	case '|': {
		do_special_identifier(tokenizer, &token, Token_Layer);
	} break;
	default: {
		if (c == '0' && *tokenizer->at == 'x') {
			advance(tokenizer);
			eat_hex(tokenizer);
			token.length = tokenizer->at - token.text;
			token.type = Token_Key_Hex;
		} else if (isdigit(c)) {
			token.type = Token_Key;
		} else if (isalpha(c)) {
			eat_identifier(tokenizer);
			token.length = tokenizer->at - token.text;
			token.type = resolve_identifier_type(token);
		} else {
			token.type = Token_Unknown;
		}
	} break;
	}

	return token;
}

void tokenizer_init(struct tokenizer *tokenizer, char *buffer) {
	tokenizer->buffer = buffer;
	tokenizer->at = buffer;
	tokenizer->line = 1;
	tokenizer->cursor = 1;
}
