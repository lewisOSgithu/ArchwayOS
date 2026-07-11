#include "os.h"

#define ATA_PRIMARY_IO 0x1F0
#define ATA_SECONDARY_IO 0x170
#define ATA_PRIMARY_CTRL 0x3F6
#define ATA_SECONDARY_CTRL 0x376
#define ATA_REG_DATA 0
#define ATA_REG_ERROR 1
#define ATA_REG_SECCOUNT0 2
#define ATA_REG_LBA0 3
#define ATA_REG_LBA1 4
#define ATA_REG_LBA2 5
#define ATA_REG_HDDEVSEL 6
#define ATA_REG_COMMAND 7
#define ATA_REG_STATUS 7
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_BSY 0x80

typedef struct {
    int present;
    int channel;
    int drive;
    uint16_t io_base;
    uint16_t ctrl_base;
    uint64_t sectors;
    char model[41];
    char serial[21];
} AtaDevice;

static AtaDevice ata_devices[4];
static int ata_ready = 0;

static int hw_streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int hw_starts(const char* s, const char* prefix) {
    while (*prefix) {
        if (*s != *prefix) return 0;
        s++;
        prefix++;
    }
    return 1;
}

static void hw_print_dec(uint64_t value) {
    char buf[21];
    int pos = 20;

    buf[pos] = 0;
    if (value == 0) {
        kprint("0");
        return;
    }
    while (value && pos > 0) {
        buf[--pos] = (char)('0' + (value % 10));
        value /= 10;
    }
    kprint(&buf[pos]);
}

static void ata_io_wait(uint16_t ctrl_base) {
    inb(ctrl_base);
    inb(ctrl_base);
    inb(ctrl_base);
    inb(ctrl_base);
}

static int ata_wait_not_busy(uint16_t io_base) {
    for (int i = 0; i < 100000; i++) {
        if ((inb(io_base + ATA_REG_STATUS) & ATA_SR_BSY) == 0) return 1;
    }
    return 0;
}

static int ata_wait_drq_or_error(uint16_t io_base) {
    for (int i = 0; i < 100000; i++) {
        uint8_t status = inb(io_base + ATA_REG_STATUS);
        if (status & ATA_SR_ERR) return 0;
        if (status & ATA_SR_DRQ) return 1;
    }
    return 0;
}

static void ata_copy_swapped(char* dst, const uint16_t* words, int start, int count) {
    int out = 0;
    for (int i = 0; i < count; i++) {
        uint16_t w = words[start + i];
        dst[out++] = (char)(w >> 8);
        dst[out++] = (char)(w & 0xFF);
    }
    dst[out] = 0;

    while (out > 0 && dst[out - 1] == ' ') {
        dst[out - 1] = 0;
        out--;
    }
}

static int ata_identify_one(int channel, int drive, uint16_t io_base, uint16_t ctrl_base, AtaDevice* out) {
    uint16_t id[256];
    uint8_t lba1;
    uint8_t lba2;

    out->present = 0;
    outb(ctrl_base, 0x02);
    outb(io_base + ATA_REG_HDDEVSEL, (uint8_t)(0xA0 | (drive << 4)));
    ata_io_wait(ctrl_base);
    outb(io_base + ATA_REG_SECCOUNT0, 0);
    outb(io_base + ATA_REG_LBA0, 0);
    outb(io_base + ATA_REG_LBA1, 0);
    outb(io_base + ATA_REG_LBA2, 0);
    outb(io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    ata_io_wait(ctrl_base);

    if (inb(io_base + ATA_REG_STATUS) == 0) return 0;
    if (!ata_wait_not_busy(io_base)) return 0;

    lba1 = inb(io_base + ATA_REG_LBA1);
    lba2 = inb(io_base + ATA_REG_LBA2);
    if (lba1 != 0 || lba2 != 0) return 0;
    if (!ata_wait_drq_or_error(io_base)) return 0;

    for (int i = 0; i < 256; i++) id[i] = inw(io_base + ATA_REG_DATA);

    out->present = 1;
    out->channel = channel;
    out->drive = drive;
    out->io_base = io_base;
    out->ctrl_base = ctrl_base;
    ata_copy_swapped(out->serial, id, 10, 10);
    ata_copy_swapped(out->model, id, 27, 20);
    out->sectors = ((uint64_t)id[61] << 16) | id[60];
    if (id[83] & (1u << 10)) {
        out->sectors =
            ((uint64_t)id[103] << 48) |
            ((uint64_t)id[102] << 32) |
            ((uint64_t)id[101] << 16) |
            id[100];
    }
    return 1;
}

void hardware_init(void) {
    ata_identify_one(0, 0, ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, &ata_devices[0]);
    ata_identify_one(0, 1, ATA_PRIMARY_IO, ATA_PRIMARY_CTRL, &ata_devices[1]);
    ata_identify_one(1, 0, ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, &ata_devices[2]);
    ata_identify_one(1, 1, ATA_SECONDARY_IO, ATA_SECONDARY_CTRL, &ata_devices[3]);
    ata_ready = 1;
}

static void cmd_drives(void) {
    int found = 0;
    if (!ata_ready) hardware_init();

    kprint("Detected ATA/IDE drives (read-only driver):\n");
    for (int i = 0; i < 4; i++) {
        AtaDevice* dev = &ata_devices[i];
        if (!dev->present) continue;
        found = 1;
        kprint("  hd");
        hw_print_dec((uint64_t)i);
        kprint(": ");
        kprint(dev->channel == 0 ? "primary " : "secondary ");
        kprint(dev->drive == 0 ? "master  " : "slave   ");
        kprint(dev->model[0] ? dev->model : "(unknown model)");
        kprint("  ");
        hw_print_dec(dev->sectors / 2048);
        kprint(" MiB\n");
    }

    if (!found) {
        kprint("  none found on legacy IDE ports.\n");
        kprint("  SATA AHCI/NVMe/USB storage needs a separate driver.\n");
    }
}

static void cmd_hd_info(void) {
    cmd_drives();
    kprint("Disk writes are intentionally disabled for hardware safety.\n");
}

int hardware_handle_command(const char* cmd) {
    if (hw_streq(cmd, "drives") || hw_streq(cmd, "lsblk")) {
        cmd_drives();
        return 1;
    }
    if (hw_streq(cmd, "hd-info") || hw_starts(cmd, "hd-info ")) {
        cmd_hd_info();
        return 1;
    }
    if (hw_streq(cmd, "hd-write") || hw_starts(cmd, "hd-write ")) {
        kprint("hd-write: disabled. This OS has read-only hardware disk support.\n");
        return 1;
    }
    if (hw_streq(cmd, "fdisk") || hw_starts(cmd, "fdisk ")) {
        kprint("fdisk: disabled. Partition editing is not implemented for safety.\n");
        return 1;
    }
    return 0;
}
