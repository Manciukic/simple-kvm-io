#include "host_io.h"

#include <linux/kvm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handle_serial(struct kvm_run *r) {
    char *data = (char *)r + r->io.data_offset;

    if (r->io.direction != KVM_EXIT_IO_OUT) return -1;

    for (unsigned i = 0; i < r->io.size * r->io.count; i++)
        putc(data[i], stdout);

    return 0;
}

static int handle_hdd_cmd(struct hdd *hdd, struct kvm_run *r,
                          void *guest_mem_addr, size_t guest_mem_size) {
    char *data = (char *)r + r->io.data_offset;
    void *guest_addr, *disk_addr;
    char cmd;

    if (r->io.size != 1) {
        return -1;
    }

    cmd = *data;

    if (hdd->op.guest_addr_off >= guest_mem_size) {
        if (hdd->status) {
            hdd->status->err = EFAULT;
        }
        return 0;
    }
    guest_addr = guest_mem_addr + hdd->op.guest_addr_off;

    if (cmd != HDD_CMD_SETUP && hdd->op.sector >= hdd->size / HDD_SECTOR_SIZE) {
        if (hdd->status) {
            hdd->status->err = EINVAL;
        }
        return 0;
    }
    disk_addr = hdd->disk_addr + (HDD_SECTOR_SIZE * hdd->op.sector);

    switch (cmd) {
        case HDD_CMD_READ:
            memcpy(guest_addr, disk_addr, HDD_SECTOR_SIZE);
            hdd->status->err = 0;
            return 0;
        case HDD_CMD_WRITE:
            memcpy(disk_addr, guest_addr, HDD_SECTOR_SIZE);
            hdd->status->err = 0;
            return 0;
        case HDD_CMD_SETUP:
            hdd->status = guest_addr;
            hdd->status->size = hdd->size;
            hdd->status->err = 0;
            return 0;
        default:
            return -1;
    }
}

static int handle_hdd_set_addr(struct hdd *hdd, struct kvm_run *r) {
    char *data = (char *)r + r->io.data_offset;

    if (r->io.size != sizeof(hdd->op.guest_addr_off)) {
        return -1;
    }

    hdd->op.guest_addr_off = *(guest_addr_t *)data;
    return 0;
}

static int handle_hdd_set_sector(struct hdd *hdd, struct kvm_run *r) {
    char *data = (char *)r + r->io.data_offset;

    if (r->io.size != sizeof(hdd->op.sector)) {
        return -1;
    }

    hdd->op.sector = *(sector_t *)data;
    return 0;
}

int handle_hdd(struct hdd *hdd, struct kvm_run *r, void *guest_mem_addr,
               size_t guest_mem_size) {
    if (r->io.direction != KVM_EXIT_IO_OUT) {
        return -1;
    }

    switch (r->io.port) {
        case HDD_CMD_PORT:
            return handle_hdd_cmd(hdd, r, guest_mem_addr, guest_mem_size);
        case HDD_DMA_ADDR_PORT:
            return handle_hdd_set_addr(hdd, r);
        case HDD_SECTOR_PORT:
            return handle_hdd_set_sector(hdd, r);
        default:
            return -1;
    }
}
