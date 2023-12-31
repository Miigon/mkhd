#pragma once

#include <Carbon/Carbon.h>

struct carbon_event {
	EventTargetRef target;
	EventHandlerUPP handler;
	EventTypeSpec type;
	EventHandlerRef handler_ref;
	char *volatile process_name;
};

char *find_process_name_for_pid(pid_t pid);
bool carbon_event_init(struct carbon_event *carbon);

char *copy_cfstring(CFStringRef string);