#include "settings.h"
#include "settings_storage.h"
#include <stddef.h>
#include <string.h>

#define SETTINGS_MAGIC 0x53455431UL
#define SETTINGS_VERSION 1U
#define SETTINGS_COMMIT 0x434F4D54U

typedef struct {
    uint32_t magic, version, sequence, size;
    DeviceSettings value;
    uint8_t reserved[32];
    uint32_t crc;
    uint32_t commit[8];
} SettingsRecord;

_Static_assert(sizeof(DeviceSettings) == 44U, "Settings layout changed: bump version");
_Static_assert(sizeof(SettingsRecord) == SETTINGS_RECORD_SIZE, "Flash record size");
_Static_assert(offsetof(SettingsRecord, commit) == SETTINGS_COMMIT_OFFSET, "Commit alignment");

static const DeviceSettings defaults = {
    "cm7-device", {192, 168, 1, 123}, {255, 255, 255, 0}, {192, 168, 1, 1}
};
static DeviceSettings current, boot, saved;
static int active_slot = -1;
static uint32_t sequence;

static uint32_t crc32(const void *data, size_t size)
{
    const uint8_t *p = data;
    uint32_t crc = UINT32_MAX;
    while (size--) {
        crc ^= *p++;
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0U - (crc & 1U)));
    }
    return ~crc;
}

static uint32_t ipv4_word(const uint8_t ip[4])
{
    return ((uint32_t)ip[0] << 24) | ((uint32_t)ip[1] << 16) |
           ((uint32_t)ip[2] << 8) | ip[3];
}

static bool parse_ipv4(const char *s, uint8_t out[4])
{
    for (unsigned i = 0; i < 4; ++i) {
        unsigned value = 0, digits = 0;
        while (*s >= '0' && *s <= '9') {
            value = value * 10U + (unsigned)(*s++ - '0');
            if (++digits > 3 || value > 255) return false;
        }
        if (!digits || (i < 3 ? *s != '.' : *s != '\0')) return false;
        out[i] = (uint8_t)value;
        if (i < 3) ++s;
    }
    return true;
}

static bool valid_name(const char *s)
{
    size_t length = 0;
    for (; length < SETTINGS_NAME_SIZE && s[length]; ++length) {
        char c = s[length];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '-')) return false;
    }
    return length > 0 && length < SETTINGS_NAME_SIZE &&
           s[0] != '-' && s[length - 1] != '-';
}

static bool unicast(const uint8_t ip[4])
{
    return ip[0] != 0 && ip[0] != 127 && ip[0] < 224;
}

bool settings_validate(const DeviceSettings *s)
{
    if (!s || !valid_name(s->name) || !unicast(s->ip)) return false;
    uint32_t mask = ipv4_word(s->mask), hostmask = ~mask;
    uint32_t ip = ipv4_word(s->ip), gateway = ipv4_word(s->gateway);
    /* Static LAN configuration: contiguous /1 through /30. */
    if (!mask || hostmask < 3 || (hostmask & (hostmask + 1U))) return false;
    if (!(ip & hostmask) || (ip & hostmask) == hostmask) return false;
    return gateway == 0 || (unicast(s->gateway) && gateway != ip &&
           (gateway & mask) == (ip & mask) && (gateway & hostmask) != 0 &&
           (gateway & hostmask) != hostmask);
}

static bool record_valid(const SettingsRecord *r)
{
    return r->commit[0] == SETTINGS_COMMIT &&
           r->commit[1] == ~SETTINGS_COMMIT &&
           r->magic == SETTINGS_MAGIC && r->version == SETTINGS_VERSION &&
           r->size == sizeof(DeviceSettings) &&
           r->crc == crc32(r, offsetof(SettingsRecord, crc)) &&
           settings_validate(&r->value);
}

void settings_init(void)
{
    SettingsRecord records[2];
    bool valid[2];
    for (unsigned i = 0; i < 2; ++i)
        valid[i] = settings_storage_read(i, &records[i]) && record_valid(&records[i]);
    active_slot = valid[0] ? 0 : -1;
    if (valid[1] && (!valid[0] ||
        (int32_t)(records[1].sequence - records[0].sequence) > 0)) active_slot = 1;
    sequence = active_slot < 0 ? 0 : records[active_slot].sequence;
    current = active_slot < 0 ? defaults : records[active_slot].value;
    boot = saved = current;
}

const DeviceSettings *settings_get(void) { return &current; }
const DeviceSettings *settings_boot(void) { return &boot; }
bool settings_loaded(void) { return active_slot >= 0; }
bool settings_dirty(void)
{
    return active_slot < 0 || memcmp(&current, &saved, sizeof(current)) != 0;
}
void settings_defaults(void) { current = defaults; }

bool settings_set(const char *key, const char *value)
{
    DeviceSettings next = current;
    if (!key || !value) return false;
    if (!strcmp(key, "name")) {
        if (!valid_name(value)) return false;
        memset(next.name, 0, sizeof(next.name));
        memcpy(next.name, value, strlen(value));
    } else {
        uint8_t *field;
        if (!strcmp(key, "ip")) field = next.ip;
        else if (!strcmp(key, "mask")) field = next.mask;
        else if (!strcmp(key, "gateway")) field = next.gateway;
        else return false;
        if (!parse_ipv4(value, field)) return false;
        if (!strcmp(key, "ip") && !unicast(field)) return false;
        if (!strcmp(key, "gateway") && ipv4_word(field) && !unicast(field)) return false;
        if (!strcmp(key, "mask")) {
            uint32_t hostmask = ~ipv4_word(field);
            if (!ipv4_word(field) || hostmask < 3 || (hostmask & (hostmask + 1U))) return false;
        }
    }
    /* Cross-field validation at save permits changing subnets in several steps. */
    current = next;
    return true;
}

bool settings_save(void)
{
    SettingsRecord record, verify;
    if (!settings_validate(&current)) return false;
    if (!settings_dirty()) return true;
    memset(&record, 0xFF, sizeof(record));
    record.magic = SETTINGS_MAGIC;
    record.version = SETTINGS_VERSION;
    record.sequence = sequence + 1U;
    record.size = sizeof(DeviceSettings);
    record.value = current;
    record.crc = crc32(&record, offsetof(SettingsRecord, crc));
    record.commit[0] = SETTINGS_COMMIT;
    record.commit[1] = ~SETTINGS_COMMIT;
    unsigned target = active_slot == 0 ? 1U : 0U;
    if (!settings_storage_write(target, &record) ||
        !settings_storage_read(target, &verify) || !record_valid(&verify) ||
        memcmp(&record, &verify, sizeof(record))) return false;
    active_slot = (int)target;
    sequence = record.sequence;
    saved = current;
    return true;
}
