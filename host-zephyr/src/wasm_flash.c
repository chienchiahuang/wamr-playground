#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/sys/crc.h>
#include "wasm_flash.h"

#define WASM_SLOT_MAGIC  0x4D534157  /* "WASM" in little-endian */
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

static const struct flash_area *slot_fa[2];
static bool initialized;

int
wasm_flash_init(void)
{
    int rc;

    rc = flash_area_open(FIXED_PARTITION_ID(wasm_a_partition), &slot_fa[0]);
    if (rc) {
        printk("[flash] failed to open slot A: %d\n", rc);
        return rc;
    }

    rc = flash_area_open(FIXED_PARTITION_ID(wasm_b_partition), &slot_fa[1]);
    if (rc) {
        printk("[flash] failed to open slot B: %d\n", rc);
        flash_area_close(slot_fa[0]);
        return rc;
    }

    printk("[flash] slot A: %u bytes, slot B: %u bytes\n",
           (unsigned)slot_fa[0]->fa_size, (unsigned)slot_fa[1]->fa_size);

    initialized = true;
    return 0;
}

static bool
read_header(int slot, struct wasm_slot_header *hdr)
{
    if (!initialized || slot < 0 || slot > 1)
        return false;

    if (flash_area_read(slot_fa[slot], 0, hdr, sizeof(*hdr)) != 0)
        return false;

    return true;
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

    if (!read_header(slot, &hdr))
        return 0;

    if (hdr.magic != WASM_SLOT_MAGIC)
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

    printk("[flash] read slot %d: %u bytes, version %u\n",
           slot, hdr.size, hdr.version);
    return hdr.size;
}

int
wasm_flash_write(int slot, const uint8_t *data, uint32_t size)
{
    if (!initialized || slot < 0 || slot > 1)
        return -1;

    if (size + DATA_OFFSET > slot_fa[slot]->fa_size) {
        printk("[flash] wasm too large for slot %d: %u > %u\n",
               slot, size, (unsigned)(slot_fa[slot]->fa_size - DATA_OFFSET));
        return -1;
    }

    uint32_t current_version = 0;
    struct wasm_slot_header existing;
    if (read_header(0, &existing) && existing.magic == WASM_SLOT_MAGIC)
        current_version = existing.version;
    if (read_header(1, &existing) && existing.magic == WASM_SLOT_MAGIC
        && existing.version > current_version)
        current_version = existing.version;

    struct wasm_slot_header hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = WASM_SLOT_MAGIC;
    hdr.size = size;
    hdr.crc32 = crc32_ieee(data, size);
    hdr.version = current_version + 1;

    int rc = flash_area_erase(slot_fa[slot], 0, slot_fa[slot]->fa_size);
    if (rc) {
        printk("[flash] erase slot %d failed: %d\n", slot, rc);
        return rc;
    }

    rc = flash_area_write(slot_fa[slot], 0, &hdr, sizeof(hdr));
    if (rc) {
        printk("[flash] write header slot %d failed: %d\n", slot, rc);
        return rc;
    }

    uint32_t write_size = ALIGN4(size);
    uint8_t pad_buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    if (write_size == size) {
        rc = flash_area_write(slot_fa[slot], DATA_OFFSET, data, size);
    } else {
        rc = flash_area_write(slot_fa[slot], DATA_OFFSET, data, size & ~3);
        if (rc == 0) {
            uint32_t tail = size & 3;
            memcpy(pad_buf, data + (size & ~3), tail);
            rc = flash_area_write(slot_fa[slot], DATA_OFFSET + (size & ~3),
                                  pad_buf, 4);
        }
    }
    if (rc) {
        printk("[flash] write data slot %d failed: %d\n", slot, rc);
        return rc;
    }

    printk("[flash] wrote slot %d: %u bytes, version %u, CRC 0x%08x\n",
           slot, size, hdr.version, hdr.crc32);
    return 0;
}
