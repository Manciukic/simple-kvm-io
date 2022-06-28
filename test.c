#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <linux/kvm.h>

#include "cpu.h"
#include "pd.h"

const char guest_fname[] = "guest.flat";

struct vm {
  int fd;
  int vcpu_mmap_size;
  uint8_t *mem;
};

int kvm_open(void)
{
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
    fprintf(stderr, "Got KVM api version %d, expected %d\n", api_ver, KVM_API_VERSION);

    return -3;
  }

  return kvm_fd;
}

int guest_mem_init(struct vm *vm, int mem_size)
{
  struct kvm_userspace_memory_region region;
  int res;

  vm->mem = mmap(0, mem_size, PROT_READ|PROT_WRITE,
                   MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  if (vm->mem == NULL) {
    perror("MAlloc(VM Mem)");

    return -1;
  }
  printf("\tAllocated guest memory (size %x) at %p\n", mem_size, vm->mem);

  region.slot = 0;
  region.flags = 0;
  region.guest_phys_addr = 0;
  region.memory_size = mem_size;
  region.userspace_addr = (unsigned long)vm->mem;
  res = ioctl(vm->fd, KVM_SET_USER_MEMORY_REGION, &region);
  if (res < 0) {
    perror("ioctl(KVM_SET_USER_MEMORY_REGION)");

    return -2;
  }

  return 0;
}

struct vm *vm_create(int mem_size)
{
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

int vcpu_create(struct vm *vm, struct kvm_run **r)
{
  int fd;

  fd = ioctl(vm->fd, KVM_CREATE_VCPU, 0);
  if (fd < 0) {
    perror("ioctl(KVM_CREATE_VCPU)");

    return -1;
  }

  *r = mmap(NULL, vm->vcpu_mmap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (r == MAP_FAILED) {
    perror("mmap kvm_run");

    return -3;
  }

  return fd;
}

#define SERIAL_PORT 0x10

int handle_serial(struct kvm_run *r) {
  char *data = (char*) r + r->io.data_offset;

  if (r->io.direction != KVM_EXIT_IO_OUT)
    return -1;

  for (unsigned i = 0; i <  r->io.size * r->io.count; i++)
    putc(data[i], stdout);

  return 0;
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

int vm_run(int fd, struct kvm_run *r)
{
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
      case KVM_EXIT_IO:
        switch (r->io.port) {
          case SERIAL_PORT:
            res = handle_serial(r);
            if (res < 0)
              return res;
            continue;
          default:
            printf("No handler defined for: "
                   "direction=%d "
                   "port=%x "
                   "size=%d\n",
                    r->io.direction, r->io.port, r->io.size);
            return -1;
        }
      default:
        fprintf(stderr,	"VM Exit reason: %d, expected KVM_EXIT_HLT (%d)\n",
                        r->exit_reason, KVM_EXIT_HLT);

        return -1;
    }
  }
}

static void setup_64bit_code_segment(struct kvm_sregs *sregs)
{
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

static int system_registers_setup(struct vm *vm, int fd)
{
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
  uint64_t *pml4 = (uint64_t *)(vm->mem + pml4_addr);

  uint64_t pdpt_addr = 0x3000;
  uint64_t *pdpt = (uint64_t *)(vm->mem + pdpt_addr);

  uint64_t pd_addr = 0x4000;
  uint64_t *pd = (uint64_t *)(vm->mem + pd_addr);


  printf("\t\t\t- PML4[0] %p (%p)...\n", pml4, vm->mem);
  fflush(stdout);
  pml4[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pdpt_addr;
  printf("\t\t\t- PDPT[0] %p (%p)...\n", pdpt, vm->mem);
  fflush(stdout);
  pdpt[0] = PDE64_PRESENT | PDE64_RW | PDE64_USER | pd_addr;
  printf("\t\t\t- PD[0]...\n");
  fflush(stdout);
  pd[0]   = PDE64_PRESENT | PDE64_RW | PDE64_USER | PDE64_PS;

  printf("\t\t- Setting up CR* and EFER...\n");
  fflush(stdout);
  sregs.cr3 = pml4_addr;
  sregs.cr4 = CR4_PAE;
  sregs.cr0 = CR0_PE | CR0_MP | CR0_ET | CR0_NE | CR0_WP | CR0_AM | CR0_PG;
  sregs.efer  = EFER_LME | EFER_LMA;

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

int registers_setup(int fd)
{
  int res;
  struct kvm_regs regs;

  memset(&regs, 0, sizeof(struct kvm_regs));
  /* Bit 1 in the rflags register must be always set. Clear all the other bits */
  regs.rflags = 2;
  /* The stack is at top 2 MB and grows down */
  regs.rsp    = 2 << 20;

  res = ioctl(fd, KVM_SET_REGS, &regs);
  if (res < 0) {
    perror("ioctl(KVM_SET_REGS)");

    return -1;
  }

  return 0;
}

int guest_config(struct vm *vm, int fd)
{
  FILE *guest_file;
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
  printf("\t\t- Opening file\n");
  fflush(stdout);
  guest_file = fopen(guest_fname, "r");
  if (guest_file == NULL) {
    perror("Cannot open guest memory file");
  }

  printf("\t\t- Getting file size\n");
  fflush(stdout);
  fseek(guest_file, 0, SEEK_END); // seek to end of file
  guest_size = ftell(guest_file); // get current file pointer
  fseek(guest_file, 0, SEEK_SET); // seek back to beginning of file

  printf("\t\t- mmap-ing file\n");
  fflush(stdout);
  guest = mmap(NULL, guest_size, PROT_READ, MAP_SHARED, fileno(guest_file), 0);

  printf("\t- Copying the guest to its memory\n");
  fflush(stdout);
  memcpy(vm->mem, guest, guest_size);

  return 0;
}


int main()
{
  int res;
  int vcpu_fd;
  struct vm *vm;
  struct kvm_run *r;

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

  printf("And running it!\n");
  fflush(stdout);
  res = vm_run(vcpu_fd, r);
  if (res != 1) {
    printf("Error: run returned %d\n", res);
    dump_registers(vcpu_fd);

    return -1;
  }

  return 0;
}
