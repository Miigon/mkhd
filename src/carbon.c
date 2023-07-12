#include "carbon.h"

#include "tr_malloc.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated"

char *copy_cfstring(CFStringRef string) {
	CFIndex num_bytes = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string), kCFStringEncodingUTF8);
	char *result = tr_malloc(num_bytes + 1);

	// NOTE(koekeishiya): Boolean: typedef -> unsigned char; false = 0, true != 0
	if (!CFStringGetCString(string, result, num_bytes + 1, kCFStringEncodingUTF8)) {
		tr_free(result);
		result = NULL;
	}

	return result;
}

static inline char *find_process_name_for_psn(ProcessSerialNumber *psn) {
	CFStringRef process_name_ref;
	if (CopyProcessName(psn, &process_name_ref) == noErr) {
		char *process_name = copy_cfstring(process_name_ref);
		for (char *s = process_name; *s; ++s)
			*s = tolower(*s);
		CFRelease(process_name_ref);
		return process_name;
	}
	return NULL;
}

inline char *find_process_name_for_pid(pid_t pid) {
	ProcessSerialNumber psn;
	GetProcessForPID(pid, &psn);
	return find_process_name_for_psn(&psn);
}

static inline char *find_active_process_name(void) {
	ProcessSerialNumber psn;
	GetFrontProcess(&psn);
	return find_process_name_for_psn(&psn);
}
#pragma clang diagnostic pop

static OSStatus carbon_event_handler(EventHandlerCallRef ref, EventRef event, void *context) {
	struct carbon_event *carbon = (struct carbon_event *)context;

	ProcessSerialNumber psn;
	if (GetEventParameter(event, kEventParamProcessID, typeProcessSerialNumber, NULL, sizeof(psn), NULL, &psn) !=
		noErr) {
		return -1;
	}

	if (carbon->process_name) {
		tr_free(carbon->process_name);
		carbon->process_name = NULL;
	}

	carbon->process_name = find_process_name_for_psn(&psn);

	return noErr;
}

bool carbon_event_init(struct carbon_event *carbon) {
	carbon->target = GetApplicationEventTarget();
	carbon->handler = NewEventHandlerUPP(carbon_event_handler);
	carbon->type.eventClass = kEventClassApplication;
	carbon->type.eventKind = kEventAppFrontSwitched;
	carbon->process_name = find_active_process_name();

	return InstallEventHandler(carbon->target, carbon->handler, 1, &carbon->type, carbon, &carbon->handler_ref) ==
		   noErr;
}
