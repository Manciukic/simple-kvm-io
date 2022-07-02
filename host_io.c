#include "host_io.h"

#include <stdlib.h>
#include <stdio.h>
#include <linux/kvm.h>

int handle_serial(struct kvm_run *r) {
  char *data = (char*) r + r->io.data_offset;

  if (r->io.direction != KVM_EXIT_IO_OUT)
    return -1;

  for (unsigned i = 0; i <  r->io.size * r->io.count; i++)
    putc(data[i], stdout);

  return 0;
}
