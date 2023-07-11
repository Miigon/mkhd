#include "hotload.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "log.h"
#include "tr_malloc.h"
#include "utils.h"

#define FSEVENT_CALLBACK(name)                                                                                         \
	void name(ConstFSEventStreamRef stream, void *context, size_t file_count, void *file_paths,                        \
			  const FSEventStreamEventFlags *flags, const FSEventStreamEventId *ids)

enum watch_kind { WATCH_KIND_INVALID, WATCH_KIND_CATALOG, WATCH_KIND_FILE };

struct watched_catalog {
	char *directory;
	char *extension;
};

struct watched_file {
	char *absolutepath;
	char *directory;
	char *filename;
};

struct watched_entry {
	enum watch_kind kind;
	union {
		struct watched_file file_info;
		struct watched_catalog catalog_info;
	};
};

static char *resolve_symlink(const char *file) {
	struct stat buffer;
	if (lstat(file, &buffer) != 0) {
		return NULL;
	}

	if (S_ISDIR(buffer.st_mode)) {
		return copy_string_malloc(file);
	}

	if (!S_ISLNK(buffer.st_mode)) {
		return copy_string_malloc(file);
	}

	char *result = realpath(file, NULL);
	return result;
}

static enum watch_kind resolve_watch_kind(char *file) {
	struct stat buffer;
	if (lstat(file, &buffer) != 0) {
		return WATCH_KIND_INVALID;
	}

	if (S_ISDIR(buffer.st_mode)) {
		return WATCH_KIND_CATALOG;
	}

	if (!S_ISLNK(buffer.st_mode)) {
		return WATCH_KIND_FILE;
	}

	return WATCH_KIND_INVALID;
}

static char *same_catalog(char *absolutepath, struct watched_catalog *catalog_info) {
	char *last_slash = strrchr(absolutepath, '/');
	if (!last_slash)
		return NULL;

	char *filename = NULL;

	// NOTE(koekeisihya): null terminate '/' to cut off filename
	*last_slash = '\0';

	if (same_string(absolutepath, catalog_info->directory)) {
		filename = !catalog_info->extension												? last_slash + 1
				   : same_string(catalog_info->extension, strrchr(last_slash + 1, '.')) ? last_slash + 1
																						: NULL;
	}

	// NOTE(koekeisihya): revert '/' to restore filename
	*last_slash = '/';

	return filename;
}

static inline bool same_file(char *absolutepath, struct watched_file *file_info) {
	bool result = same_string(absolutepath, file_info->absolutepath);
	return result;
}

static FSEVENT_CALLBACK(hotloader_handler) {
	/* NOTE(koekeishiya): We sometimes get two events upon file save. */
	struct hotloader *hotloader = (struct hotloader *)context;
	char **files = (char **)file_paths;

	for (unsigned file_index = 0; file_index < file_count; ++file_index) {
		for (unsigned watch_index = 0; watch_index < hotloader->watch_count; ++watch_index) {
			struct watched_entry *watch_info = hotloader->watch_list + watch_index;
			if (watch_info->kind == WATCH_KIND_CATALOG) {
				char *filename = same_catalog(files[file_index], &watch_info->catalog_info);
				if (!filename)
					continue;

				hotloader->callback(files[file_index], watch_info->catalog_info.directory, filename);
				break;
			} else if (watch_info->kind == WATCH_KIND_FILE) {
				bool match = same_file(files[file_index], &watch_info->file_info);
				if (!match)
					continue;

				hotloader->callback(watch_info->file_info.absolutepath, watch_info->file_info.directory,
									watch_info->file_info.filename);
				break;
			}
		}
	}
}

static inline void hotloader_add_watched_entry(struct hotloader *hotloader, struct watched_entry entry) {
	if (!hotloader->watch_list) {
		hotloader->watch_capacity = 32;
		hotloader->watch_list =
			(struct watched_entry *)tr_malloc(hotloader->watch_capacity * sizeof(struct watched_entry));
	}

	if (hotloader->watch_count >= hotloader->watch_capacity) {
		hotloader->watch_capacity = (unsigned)ceil(hotloader->watch_capacity * 1.5f);
		hotloader->watch_list = (struct watched_entry *)tr_realloc(
			hotloader->watch_list, hotloader->watch_capacity * sizeof(struct watched_entry));
	}

	hotloader->watch_list[hotloader->watch_count++] = entry;
}

bool hotloader_add_catalog(struct hotloader *hotloader, const char *directory, const char *extension) {
	if (hotloader->enabled)
		return false;

	char *real_path = resolve_symlink(directory);
	if (!real_path)
		return false;

	enum watch_kind kind = resolve_watch_kind(real_path);
	if (kind != WATCH_KIND_CATALOG)
		return false;

	hotloader_add_watched_entry(
		hotloader,
		(struct watched_entry){
			.kind = WATCH_KIND_CATALOG,
			.catalog_info = {.directory = real_path, .extension = extension ? copy_string_malloc(extension) : NULL}});

	return true;
}

bool hotloader_add_file(struct hotloader *hotloader, const char *file) {
	if (hotloader->enabled)
		return false;

	char *real_path = resolve_symlink(file);
	if (!real_path)
		return false;

	enum watch_kind kind = resolve_watch_kind(real_path);
	if (kind != WATCH_KIND_FILE)
		return false;

	hotloader_add_watched_entry(hotloader, (struct watched_entry){.kind = WATCH_KIND_FILE,
																  .file_info = {.absolutepath = real_path,
																				.directory = file_directory(real_path),
																				.filename = file_name(real_path)}});

	return true;
}

void hotloader_debug(struct hotloader *hotloader) {
	for (int i = 0; i < hotloader->watch_count; ++i) {
		debug("\t%s\n", hotloader->watch_list[i].file_info.absolutepath);
	}
}

bool hotloader_begin(struct hotloader *hotloader, hotloader_callback *callback) {
	if (hotloader->enabled || !hotloader->watch_count)
		return false;

	CFStringRef string_refs[hotloader->watch_count];
	for (unsigned index = 0; index < hotloader->watch_count; ++index) {
		struct watched_entry *watch_info = hotloader->watch_list + index;
		char *directory =
			watch_info->kind == WATCH_KIND_FILE ? watch_info->file_info.directory : watch_info->catalog_info.directory;
		string_refs[index] = CFStringCreateWithCString(kCFAllocatorDefault, directory, kCFStringEncodingUTF8);
	}

	FSEventStreamContext context = {.info = hotloader};

	hotloader->path =
		(CFArrayRef)CFArrayCreate(NULL, (const void **)string_refs, hotloader->watch_count, &kCFTypeArrayCallBacks);

	hotloader->flags = kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents;

	hotloader->stream = FSEventStreamCreate(NULL, hotloader_handler, &context, hotloader->path,
											kFSEventStreamEventIdSinceNow, 0.5, hotloader->flags);

	FSEventStreamScheduleWithRunLoop(hotloader->stream, CFRunLoopGetMain(), kCFRunLoopDefaultMode);

	hotloader->callback = callback;
	FSEventStreamStart(hotloader->stream);
	hotloader->enabled = true;

	return true;
}

void hotloader_end(struct hotloader *hotloader) {
	if (!hotloader->enabled)
		return;

	// assuming: tr_malloc-ed resources already freed by `trctx_free_everything()`.
	// so here we don't free any tr_malloc-ed pointers in hotloader.

	FSEventStreamStop(hotloader->stream);
	FSEventStreamInvalidate(hotloader->stream);
	FSEventStreamRelease(hotloader->stream);

	CFIndex count = CFArrayGetCount(hotloader->path);
	for (unsigned index = 0; index < count; ++index) {
		struct watched_entry *watch_info = hotloader->watch_list + index;
		CFRelease(CFArrayGetValueAtIndex(hotloader->path, index));
	}

	CFRelease(hotloader->path);
	memset(hotloader, 0, sizeof(struct hotloader));
}
