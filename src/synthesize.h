#pragma once

#include <stdbool.h>

struct keyevent;

void synthesize_key(struct keyevent *keyevent);
void synthesize_key_list(struct keyevent *keyevent, bool nore);
bool parse_and_synthesize_key(char *key_string, bool nore);
void synthesize_text(char *text);
