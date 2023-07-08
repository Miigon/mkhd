#pragma once

#include <stdint.h>
#include <stdbool.h>

#define CF_NOTIFICATION_CALLBACK(name) \
    void name(CFNotificationCenterRef center, \
              void *observer, \
              CFNotificationName name, \
              const void *object, \
              CFDictionaryRef userInfo)
typedef CF_NOTIFICATION_CALLBACK(cf_notification_callback);

bool initialize_keycode_map(void);
uint32_t keycode_from_char(char key);

