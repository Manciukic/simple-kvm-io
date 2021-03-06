#include <errno.h>
#include <fcntl.h>
#include <linux/kvm.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "cpu.h"
#include "host_io.h"
#include "pd.h"

const char guest_fname[] = "guest.flat";
const char hdd_fname[] = "disk.raw";

struct vm_mem {
    uint8_t *addr;
    size_t size;
};

struct vm {
    int fd;
    int vcpu_mmap_size;
    struct vm_mem mem;
};

int kvm_open(void) {
    int kvm_fd;
    int api_ver;

    kvm_fd = open("/dev/kvm", O_RDWR);
    if (kvm_fd < 0) {
        perror("open(/dev/kvm)");

        return -1;
    }

    api_ver = ioctl(kvm_fd, KVM_GET_API_VERSION, 0);
    if (api_ver < 0) {
        perror("ioctl(KVM_GET_API_VERSION)");

        return -2;
    }

    if (api_ver != KVM_API_VERSION) {
        fprintf(stderr, "Got KVM api version %d, expected %d\n", api_ver,
                KVM_API_VERSION);

        return -3;
    }

    return kvm_fd;
}

int guest_mem_init(struct vm *vm, int mem_size) {
    struct kvm_userspace_memory_region region;
    int res;

    vm->mem.addr = mmap(0, mem_size, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (vm->mem.addr == NULL) {
        perror("MAlloc(VM Mem)");

        return -1;
    }
    vm->mem.size = mem_size;
    printf("\tAllocated guest memory (size %lx) at %p\n", vm->mem.size,
           vm->mem.addr);
    region.slot = 0;
    region.flags = 0;
    region.guest_phys_addr = 0;
    region.memory_size = vm->mem.size;
    region.userspace_addr = (unsigned long)vm->mem.addr;
    res = ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &region);
    if (res < 0) {
        perror("ioctl(KVM_SET_USER_MEMORY_REGION)");

        return -2;
    }

    return 0;
}

struct vm *vm_create(int mem_size) {
    int kvm_fd;
    struct vm *vm;

    kvm_fd = kvm_open();
    if (kvm_fd < 0) {
        return NULL;
    }

    vm = malloc(sizeof(struct vm));
    if (vm == NULL) {
        perror("MAlloc(vm)");

        return NULL;
    }
    vm->fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
    if (vm->fd < 0) {
        perror("ioctl(KVM_CREATE_VM)");

        return NULL;
    }
    vm->vcpu_mmap_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (vm->vcpu_mmap_size <= 0) {
        perror("ioctl(KVM_GET_VCPU_MMAP_SIZE)");

        return NULL;
    }

    if (guest_mem_init(vm, mem_size) < 0) {
        return NULL;
    }

    return vm;
}

int vcpu_create(struct vm *vm, struct kvm_run **r) {
    int fd;

    fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
    if (fd < 0) {
        perror("ioctl(KVM_CREATE_VCPU)");

        return -1;
    }

    *r = mmap(NULL, vm->vcpu_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
              0);
    if (r == MAP_FAILED) {
        perror("mmap kvm_run");

        return -3;
    }

    return fd;
}

int dump_registers(int fd) {
    struct kvm_regs regs;
    int res;

    res = ioctl(fd, KVM_GET_REGS, &regs);
    if (res < 0) {
        perror("ioctl(KVM_GET_SREGS)");

        return -1;
    }

    printf("rax: %llx\n", regs.rax);
    printf("rbx: %llx\n", regs.rbx);
    printf("rcx: %llx\n", regs.rcx);
    printf("rdx: %llx\n", regs.rdx);
    printf("rsi: %llx\n", regs.rsi);
    printf("rdi: %llx\n", regs.rdi);
    printf("rsp: %llx\n", regs.rsp);
    printf("rbp: %llx\n", regs.rbp);
    printf("r8: %llx\n", regs.r8);
    printf("r9: %llx\n", regs.r9);
    printf("r10: %llx\n", regs.r10);
    printf("r11: %llx\n", regs.r11);
    printf("r12: %llx\n", regs.r12);
    printf("r13: %llx\n", regs.r13);
    printf("r14: %llx\n", regs.r14);
    printf("r15: %llx\n", regs.r15);
    printf("rip: %llx\n", regs.rip);
    printf("rflags: %llx\n", regs.rflags);
    fflush(stdout);

    return 0;
}

int vm_run(int fd, struct kvm_run *r, struct vm_mem *mem, struct hdd *h) {
    struct kvm_regs regs;
    int res;

    for (;;) {
        res = ioctl(fd, KVM_RUN, 0);
        if (res < 0) {
            perror("ioctl(KVM_RUN)");

            return -1;
        }

        switch (r->exit_reason) {
            case KVM_EXIT_HLT:
                res = ioctl(fd, KVM_GET_REGS, &regs);
                if (res < 0) {
                    perror("ioctl(KVM_GET_REGS)");

                    return -1;
                }

                printf("EAX: %llx\n", regs.rax);
                printf("EDX: %llx\n", regs.rdx);

                return 1;
            case KVM_EXIT_MMIO:
                if (!r->mmio.is_write) {
                    printf("Unhandled MMIO read request!\n");
                    return -1;
                }

                // emulate a normal io out, so that I don't need to rewrite the
                // functions
                __u64 phys_addr = r->mmio.phys_addr;
                __u32 len = r->mmio.len;
                // copy the data in the unused space after the io struct
                int data_offset = offsetof(struct kvm_run, io) + sizeof(r->io);
                __u8 *data = (__u8*) r + data_offset;

                memcpy(data, r->mmio.data, 8);
                r->io.count = 1;
                r->io.data_offset = data_offset;
                r->io.direction = KVM_EXIT_IO_OUT;
                r->io.port = (phys_addr - MMIO_ADDR) / 8;
                r->io.size = len;
                // fall through
            case KVM_EXIT_IO:
                switch (r->io.port) {
                    case SERIAL_PORT:
                        res = handle_serial(r);
                        if (res < 0) return res;
                        continue;
                    case HDD_CMD_PORT:
                    case HDD_DMA_ADDR_PORT:
                    case HDD_SECTOR_PORT:
                        res = handle_hdd(h, r, mem->addr, mem->size);
                        if (res < 0) return res;
                        continue;
                    default:
                        printf(
                            "No handler defined for: "
                            "direction=%d "
                            "port=%x "
                            "size=%d\n",
                            r->io.direction, r->io.port, r->io.size);
                        return -1;
                }
            default:
                fprintf(stderr,
                        "VM Exit reason: %d, expected KVM_EXIT_HLT (%d)\n",
                        r->exit_reason, KVM_EXIT_HLT);

                return -1;
        }
    }
}

static void setup_64bit_code_segment(struct kvm_sregs *sregs) {
    struct kvm_segment seg = {
        .base = 0,
        .limit = 0xffffffff,
        .selector = 1 << 3,
        .present = 1,
        .type = 11, /* Code: execute, read, accessed */
        .dpl = 0,
        .db = 0,
        .s = 1, /* Code/data */
        .l = 1,
        .g = 1, /* 4KB granularity */
    };

    printf("\t\t\t- Setting CS\n");
    fflush(stdout);
    sregs->cs = seg;

    seg.type = 3; /* Data: read/write, accessed */
    seg.selector = 2 << 3;
    printf("\t\t\t- Setting {D,E,F,G,S}S\n");
    fflush(stdout);
    sregs->ds = sregs->es = sregs->fs = sregs->gs = sregs->ss = seg;
}

static int system_registers_setup(struct vm *vm, int fd) {
    int res;
    struct kvm_sregs sregs;

    printf("\t\t- Reading system registers\n");
    fflush(stdout);
    res = ioctl(fd, KVM_GET_SREGS, &sregs);
    if (res < 0) {
        perror("ioctl(KVM_GET_SREGS)");

        return -1;
    }

    printf("\t\t- Setting up page tables...\n");
    fflush(stdout);
    uint64_t pml4_addr = 0x2000;
    uint64_t *pml4 = (uint64_t *)(vm->mem.addr + pml4_addr);

    uint64_t pdpt_addr = 0x3000;
    uint64_t *pdpt = (uint64_t *)(vm->mem.addr + pdpt_addr);

    uint64_t pd_addr = 0x4000;
    uint64_t *pd = (uint64_t *)(vm->mem.addr + pd_addr);

    printf("\t\t\t- PML4[0] %p (%p)...\n", pml4, vm->mem.addr);
    fflush(stdout);
    pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr;
    printf("\t\t\t- PDPT[0] %p (%p)...\n", pdpt, vm->mem.addr);
    fflush(stdout);
    pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;
    printf("\t\t\t- PD[0,1]...\n");
    fflush(stdout);
    pd[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;
    // io mem has cache disabled
    pd[1] = PDE64_PRESENT | PDE64_RW | PDE64_USER
            | PDE64_PWT | PDE64_PCD |  PDE64_PS
            | MMIO_ADDR; // io mem is 0x200000-0x3fffff
    printf("\t\t- Setting up CR* and EFER...\n");
    fflush(stdout);
    sregs.cr3 = pml4_addr;
    sregs.cr4 = CR4_PAE;
    sregs.cr0 = CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
    sregs.efer = EFER_LME | EFER_LMA;

    setup_64bit_code_segment(&sregs);

    printf("\t\t- Writing back system registers\n");
    fflush(stdout);
    res = ioctl(fd, KVM_SET_SREGS, &sregs);
    if (res < 0) {
        perror("ioctl(KVM_SET_SREGS)");

        return -2;
    }

    return 0;
}

int registers_setup(int fd) {
    int res;
    struct kvm_regs regs;

    memset(&regs, 0, sizeof(struct kvm_regs));
    /* Bit 1 in the rflags register must be always set. Clear all the other bits
     */
    regs.rflags = 2;
    /* The stack is at top 2 MB and grows down */
    regs.rsp = 2 << 20;

    res = ioctl(fd, KVM_SET_REGS, &regs);
    if (res < 0) {
        perror("ioctl(KVM_SET_REGS)");

        return -1;
    }

    return 0;
}

static int mmap_file(const char *fname, void **addr, const char *modes) {
    FILE *file;
    int size, prot = 0;

    file = fopen(fname, modes);
    if (file == NULL) {
        perror("Cannot open file");
        return -1;
    }

    fseek(file, 0, SEEK_END);  // seek to end of file
    size = ftell(file);        // get current file pointer
    fseek(file, 0, SEEK_SET);  // seek back to beginning of file

    if (strcmp(modes, "r") == 0) prot = PROT_READ;
    if (strcmp(modes, "r+") == 0) prot = PROT_READ | PROT_WRITE;

    *addr = mmap(NULL, size, prot, MAP_SHARED, fileno(file), 0);
    if (*addr == MAP_FAILED) {
        perror("mmap_file");
        return -1;
    }

    return size;
}

int guest_config(struct vm *vm, int fd) {
    int guest_size;
    void *guest;

    printf("\t- Setting up system registers\n");
    fflush(stdout);
    if (system_registers_setup(vm, fd) < 0) {
        return -9;
    }

    printf("\t- Setting up user registers\n");
    fflush(stdout);
    if (registers_setup(fd) < 0) {
        return -10;
    }

    printf("\t- Loading guest memory\n");
    fflush(stdout);
    guest_size = mmap_file(guest_fname, &guest, "r");
    if (guest_size < 0) return -1;

    printf("\t- Copying the guest to its memory\n");
    fflush(stdout);
    memcpy(vm->mem.addr, guest, guest_size);

    return 0;
}

static struct hdd *setup_hdd(const char *fname) {
    struct hdd *h = (struct hdd *)calloc(1, sizeof(struct hdd));
    int res;

    res = mmap_file(fname, &h->disk_addr, "r+");
    if (res < 0) return NULL;
    h->size = res;

    if (h->size % HDD_SECTOR_SIZE != 0) {
        printf("disk must be a multiple of %d in size ", HDD_SECTOR_SIZE);
        return NULL;
    }

    return h;
}

int main() {
    int res;
    int vcpu_fd;
    struct vm *vm;
    struct kvm_run *r;
    struct hdd *h;

    printf("Simple kvm test...\n");
    fflush(stdout);
    vm = vm_create(0x200000);
    if (vm == NULL) {
        return -1;
    }
    printf("Creating VCPU...\n");
    fflush(stdout);
    vcpu_fd = vcpu_create(vm, &r);

    printf("Configuring the guest...\n");
    fflush(stdout);
    res = guest_config(vm, vcpu_fd);
    if (res < 0) {
        return -1;
    }

    printf("Configuring the disk...\n");
    fflush(stdout);
    h = setup_hdd(hdd_fname);
    if (h == NULL) {
        return -1;
    }

    printf("And running it!\n");
    fflush(stdout);
    res = vm_run(vcpu_fd, r, &vm->mem, h);
    if (res != 1) {
        printf("Error: run returned %d\n", res);
        dump_registers(vcpu_fd);

        return -1;
    }

    return 0;
}
