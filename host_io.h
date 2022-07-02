#include <linux/kvm.h>
#include <stdlib.h>
#include "io.h"

extern int handle_serial(struct kvm_run *r);

#define sector_t unsigned int
#define guest_addr_t unsigned int
struct hdd {
    void* disk_addr;
    size_t size;
    struct {
        guest_addr_t guest_addr_off;
        sector_t sector;
    } op;
    struct hdd_status *status;
};

extern int handle_hdd(struct hdd *hdd, struct kvm_run *r, void* guest_mem_addr,
                    size_t guest_mem_size);
