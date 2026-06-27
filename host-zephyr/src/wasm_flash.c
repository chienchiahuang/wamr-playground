#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include "wasm_flash.h"

#define WASM_SLOT_MAGIC  0x4D534157
#define HEADER_SIZE      32
#define DATA_OFFSET      HEADER_SIZE
#define ALIGN4(x)        (((x) + 3) & ~3)

struct wasm_slot_header {
    uint32_t magic;
    uint32_t size;
    uint32_t crc32;
    uint32_t version;
    uint8_t  reserved[16];
};

static const struct flash_area *slot_fa[WASM_NUM_SLOTS];
static bool initialized;

static const int slot_partition_ids[WASM_NUM_SLOTS] = {
    FIXED_PARTITION_ID(wasm_0_partition),
    FIXED_PARTITION_ID(wasm_1_partition),
    FIXED_PARTITION_ID(wasm_2_partition),
};

int
wasm_flash_init(void)
{
    for (int i = 0; i < WASM_NUM_SLOTS; i++) {
        int rc = flash_area_open(slot_partition_ids[i], &slot_fa[i]);
        if (rc) {
            printk("[flash] failed to open slot %d: %d\n", i, rc);
            return rc;
        }
    }

    printk("[flash] %d slots ready (%u bytes each)\n",
           WASM_NUM_SLOTS, (unsigned)slot_fa[0]->fa_size);

    initialized = true;
    return 0;
}

static bool
read_header(int slot, struct wasm_slot_header *hdr)
{
    if (!initialized || slot < 0 || slot >= WASM_NUM_SLOTS)
        return false;
    return flash_area_read(slot_fa[slot], 0, hdr, sizeof(*hdr)) == 0;
}

bool
wasm_flash_valid(int slot)
{
    struct wasm_slot_header hdr;

    if (!read_header(slot, &hdr))
        return false;

    if (hdr.magic != WASM_SLOT_MAGIC)
        return false;

    if (hdr.size == 0 || hdr.size > slot_fa[slot]->fa_size - DATA_OFFSET)
        return false;

    uint8_t buf[256];
    uint32_t crc = 0;
    uint32_t remaining = hdr.size;
    uint32_t offset = DATA_OFFSET;

    while (remaining > 0) {
        uint32_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
        if (flash_area_read(slot_fa[slot], offset, buf, chunk) != 0)
            return false;
        crc = crc32_ieee_update(crc, buf, chunk);
        offset += chunk;
        remaining -= chunk;
    }

    return crc == hdr.crc32;
}

uint32_t
wasm_flash_version(int slot)
{
    struct wasm_slot_header hdr;
    if (!read_header(slot, &hdr) || hdr.magic != WASM_SLOT_MAGIC)
        return 0;
    return hdr.version;
}

uint32_t
wasm_flash_read(int slot, uint8_t *buf, uint32_t buf_size)
{
    struct wasm_slot_header hdr;
    if (!read_header(slot, &hdr))
        return 0;
    if (hdr.magic != WASM_SLOT_MAGIC || hdr.size == 0)
        return 0;
    if (hdr.size > buf_size)
        return 0;
    if (flash_area_read(slot_fa[slot], DATA_OFFSET, buf, hdr.size) != 0)
        return 0;

    uint32_t crc = crc32_ieee(buf, hdr.size);
    if (crc != hdr.crc32) {
        printk("[flash] slot %d CRC mismatch\n", slot);
        return 0;
    }

    return hdr.size;
}

int
wasm_flash_write(int slot, const uint8_t *data, uint32_t size)
{
    if (!initialized || slot < 0 || slot >= WASM_NUM_SLOTS)
        return -1;

    if (size + DATA_OFFSET > slot_fa[slot]->fa_size)
        return -1;

    struct wasm_slot_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = WASM_SLOT_MAGIC;
    hdr.size = size;
    hdr.crc32 = crc32_ieee(data, size);
    hdr.version = wasm_flash_version(slot) + 1;

    int rc = flash_area_erase(slot_fa[slot], 0, slot_fa[slot]->fa_size);
    if (rc) return rc;

    rc = flash_area_write(slot_fa[slot], 0, &hdr, sizeof(hdr));
    if (rc) return rc;

    uint32_t write_size = ALIGN4(size);
    if (write_size == size) {
        rc = flash_area_write(slot_fa[slot], DATA_OFFSET, data, size);
    } else {
        rc = flash_area_write(slot_fa[slot], DATA_OFFSET, data, size & ~3);
        if (rc == 0) {
            uint8_t pad[4] = {0xFF, 0xFF, 0xFF, 0xFF};
            memcpy(pad, data + (size & ~3), size & 3);
            rc = flash_area_write(slot_fa[slot], DATA_OFFSET + (size & ~3), pad, 4);
        }
    }
    if (rc) return rc;

    printk("[flash] wrote slot %d: %u bytes, v%u\n", slot, size, hdr.version);
    return 0;
}
