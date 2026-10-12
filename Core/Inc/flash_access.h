#ifndef APP_FLASH_ACCESS_H
#define APP_FLASH_ACCESS_H
#include <stdbool.h>
/* All CM7 Flash writers must share this lock, including the TFTP example.
 * CM4 must not write Flash concurrently; this mutex is local to CM7. */
bool flash_access_init(void);
bool flash_access_take(void);
void flash_access_give(void);
#endif
