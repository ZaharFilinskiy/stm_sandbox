#include "settings.h"
#include "settings_storage.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unsigned char flash[2][SETTINGS_RECORD_SIZE];
static int cut_after = -1;
static unsigned writes;

bool settings_storage_read(unsigned slot, void *record)
{
    assert(slot < 2);
    memcpy(record, flash[slot], SETTINGS_RECORD_SIZE);
    return true;
}

bool settings_storage_write(unsigned slot, const void *record)
{
    assert(slot < 2);
    ++writes;
    memset(flash[slot], 0xFF, SETTINGS_RECORD_SIZE);
    size_t length = cut_after < 0 ? SETTINGS_RECORD_SIZE : (unsigned)cut_after;
    memcpy(flash[slot], record, length);
    return cut_after < 0;
}

static void fresh(void)
{
    memset(flash, 0xFF, sizeof(flash));
    cut_after = -1;
    writes = 0;
    settings_init();
}

int main(void)
{
    fresh();
    assert(!settings_loaded() && settings_dirty());
    assert(settings_validate(settings_get()));
    assert(settings_get()->ip[3] == 123);
    assert(settings_save() && !settings_dirty() && writes == 1);
    assert(settings_save() && writes == 1); /* No wear for unchanged values. */

    const char *invalid_ips[] = {"", "1.2.3", "1.2.3.4.5", "256.1.1.1", "-1.2.3.4",
        "1.2.3.4x", " 1.2.3.4", "999999999999.1.1.1", "127.0.0.1", "224.0.0.1"};
    DeviceSettings old = *settings_get();
    for (unsigned i = 0; i < sizeof(invalid_ips) / sizeof(invalid_ips[0]); ++i) {
        assert(!settings_set("ip", invalid_ips[i]));
        assert(!memcmp(settings_get(), &old, sizeof(old)));
    }
    assert(!settings_set("mask", "255.0.255.0"));
    assert(!settings_set("mask", "255.255.255.255"));
    assert(!settings_set("mask", "0.0.0.0"));
    assert(!settings_set("name", ""));
    assert(!settings_set("name", "bad name"));
    assert(!settings_set("name", "-bad"));
    assert(!settings_set("name", "bad-"));
    assert(!settings_set("name", "01234567890123456789012345678901"));
    assert(!settings_set("unknown", "1"));
    assert(!settings_set(NULL, "1"));
    assert(!settings_set("ip", NULL));

    assert(settings_set("ip", "10.20.30.40"));
    assert(!settings_validate(settings_get())); /* Old gateway, new subnet. */
    assert(!settings_save() && writes == 1);
    assert(settings_set("gateway", "10.20.30.1"));
    assert(settings_set("name", "board-7"));
    assert(settings_validate(settings_get()));
    assert(settings_boot()->ip[0] == 192); /* Boot snapshot is immutable. */
    assert(settings_save() && writes == 2);
    settings_init();
    assert(settings_loaded() && !settings_dirty());
    assert(!strcmp(settings_get()->name, "board-7"));
    assert(settings_boot()->ip[0] == 10);
    assert(settings_set("ip", "10.20.30.0") && !settings_save());
    assert(settings_set("ip", "10.20.30.255") && !settings_save());
    assert(settings_set("ip", "10.20.30.40"));
    assert(settings_set("gateway", "10.20.30.40") && !settings_save());
    assert(settings_set("gateway", "0.0.0.0") && settings_save());
    settings_defaults();
    assert(settings_dirty() && settings_save());
    settings_init();
    assert(settings_get()->ip[0] == 192);

    /* Corrupt latest copy: use previous valid copy. Then corrupt both. */
    fresh();
    assert(settings_save());
    assert(settings_set("name", "new") && settings_save());
    flash[1][20] ^= 0x80;
    settings_init();
    assert(settings_loaded() && !strcmp(settings_get()->name, "cm7-device"));
    flash[0][0] ^= 1;
    settings_init();
    assert(!settings_loaded() && settings_dirty());

    /* Every possible interrupted write, including a partially written commit.
     * Accept only a complete old or complete new configuration after restart. */
    for (unsigned cut = 0; cut <= SETTINGS_RECORD_SIZE; ++cut) {
        fresh();
        assert(settings_save());
        assert(settings_set("name", "after-power-cut"));
        cut_after = (int)cut;
        assert(!settings_save());
        settings_init();
        assert(settings_loaded() && settings_validate(settings_get()));
        assert(!strcmp(settings_get()->name, "cm7-device") ||
               !strcmp(settings_get()->name, "after-power-cut"));
        if (cut < SETTINGS_COMMIT_OFFSET + 8U)
            assert(!strcmp(settings_get()->name, "cm7-device"));
        cut_after = -1;
        assert(settings_set("name", "recovered") && settings_save());
        settings_init();
        assert(!strcmp(settings_get()->name, "recovered"));
    }
    puts("PASS: validation, persistence, boot snapshot, CRC fallback, 129 torn writes and recovery");
    return 0;
}
