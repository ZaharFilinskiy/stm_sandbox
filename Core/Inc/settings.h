#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#define SETTINGS_NAME_SIZE 32U
typedef struct {
    char name[SETTINGS_NAME_SIZE];
    uint8_t ip[4];
    uint8_t mask[4];
    uint8_t gateway[4];
} DeviceSettings;

/* Init before creating tasks. Thereafter the CLI owns the editable settings.
 * The network task uses the immutable boot snapshot. */
void settings_init(void);
const DeviceSettings *settings_get(void);
const DeviceSettings *settings_boot(void);
bool settings_loaded(void);
bool settings_dirty(void);
bool settings_set(const char *key, const char *value);
bool settings_validate(const DeviceSettings *settings);
void settings_defaults(void);
bool settings_save(void);

#endif
