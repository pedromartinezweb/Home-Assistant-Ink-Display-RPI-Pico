#include "device_store.h"

#include <stddef.h>
#include <string.h>

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include "pico/platform.h"

enum {
    STORE_MAGIC = 0x314b4e49,
    STORE_VERSION = 1
};

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t provisioning_id;
    uint16_t port;
    uint8_t paired;
    uint8_t reserved;
    char host[INK_HOST_MAX + 1];
    char path[INK_PATH_MAX + 1];
    uint8_t secret[INK_SECRET_SIZE];
    uint32_t checksum;
} StoreRecord;

typedef struct {
    uint32_t offset;
    uint8_t page[FLASH_PAGE_SIZE];
} StoreWrite;

static uint32_t checksum(const uint8_t *data, size_t size) {
    uint32_t value = 2166136261U;
    for (size_t index = 0; index < size; ++index) {
        value ^= data[index];
        value *= 16777619U;
    }
    return value;
}

static uint32_t store_offset(void) {
    return PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;
}

static const StoreRecord *stored(void) {
    return (const StoreRecord *)(XIP_BASE + store_offset());
}

static bool record_valid(const StoreRecord *record, uint32_t provisioning_id) {
    return record->magic == STORE_MAGIC &&
           record->version == STORE_VERSION &&
           record->size == sizeof(*record) &&
           record->provisioning_id == provisioning_id &&
           record->paired == 1 &&
           record->host[0] != '\0' &&
           record->path[0] == '/' &&
           record->port != 0 &&
           checksum((const uint8_t *)record, offsetof(StoreRecord, checksum)) == record->checksum;
}

static void __not_in_flash_func(write_record)(void *context) {
    StoreWrite *write = context;
    flash_range_erase(write->offset, FLASH_SECTOR_SIZE);
    flash_range_program(write->offset, write->page, sizeof(write->page));
}

void device_store_load(uint32_t provisioning_id, DeviceSettings *settings) {
    if (settings == NULL) {
        return;
    }
    memset(settings, 0, sizeof(*settings));
    settings->provisioning_id = provisioning_id;
    const StoreRecord *record = stored();
    if (!record_valid(record, provisioning_id)) {
        return;
    }
    settings->paired = true;
    settings->port = record->port;
    memcpy(settings->host, record->host, sizeof(settings->host));
    memcpy(settings->path, record->path, sizeof(settings->path));
    memcpy(settings->secret, record->secret, sizeof(settings->secret));
}

bool device_store_pair(uint32_t provisioning_id,
                       const InkPairRequest *request,
                       DeviceSettings *settings) {
    if (request == NULL || settings == NULL) {
        return false;
    }
    StoreRecord record;
    memset(&record, 0, sizeof(record));
    record.magic = STORE_MAGIC;
    record.version = STORE_VERSION;
    record.size = sizeof(record);
    record.provisioning_id = provisioning_id;
    record.port = request->port;
    record.paired = 1;
    memcpy(record.host, request->host, sizeof(record.host));
    memcpy(record.path, request->path, sizeof(record.path));
    memcpy(record.secret, request->secret, sizeof(record.secret));
    record.checksum = checksum((const uint8_t *)&record, offsetof(StoreRecord, checksum));

    StoreWrite write;
    write.offset = store_offset();
    memset(write.page, 0xff, sizeof(write.page));
    memcpy(write.page, &record, sizeof(record));
    if (flash_safe_execute(write_record, &write, 5000) != PICO_OK) {
        return false;
    }
    device_store_load(provisioning_id, settings);
    return settings->paired;
}
