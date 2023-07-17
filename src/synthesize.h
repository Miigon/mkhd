#pragma once

#include <stdbool.h>

struct keyevent;

bool synthesize_key(struct keyevent *keyevent);
bool parse_and_synthesize_key(char *key_string);
void synthesize_text(char *text);
