#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Two independently erasable slots. The final 32-byte word is the commit. */
#define SETTINGS_RECORD_SIZE 128U
#define SETTINGS_COMMIT_OFFSET 96U
bool settings_storage_read(unsigned slot, void *record);
bool settings_storage_write(unsigned slot, const void *record);

#endif
