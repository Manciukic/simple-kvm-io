# Simple KVM I/O

Small hands-on project for the class in Advanced OS at Scuola Superiore
Sant'Anna held by Prof. L. Abeni [1].
The project expands a "hello world" example for KVM virtualization [2] by adding
two simple I/O devices: a serial port, and a simple "disk".
Other improvements over the starting example are:
 - guest code is loaded from file
 - fix bug in guest memory allocation (`aligned_alloc` arguments are swapped!)
 - guest code is compiled from C

By compiling with `MMIO=true` you can use memory-mapped I/O instead of port I/O.

[1] http://retis.santannapisa.it/luca/SOAvanzati/
[2] http://retis.santannapisa.it/luca/SOAvanzati/Src/kvm-test.tgz

## Usage

```
make      # compiles the code (port I/O)
# or `make MMIO=true`

make disk # creates empty disk (8KiB)

./test    # runs hypervisor and guest
```

## Specification

### Serial port

The serial port makes it possible to send messages from the guest to the host.
These messages will be print to stdout by the vmm.

| port | direction | description                                               |
| ---- | --------- | --------------------------------------------------------- |
| 0x10 |    out    | write the payload to the serial port                      |

### Simple disk

The "disk" is a device which loads disk sectors (512B) to the VM memory through
DMA and is controlled by three IO ports:
 - the first sets the sector (512B) offset.
 - the second sets the memory address for the DMA
 - the third sends the operation to perform (0: read; 1: write, 2: setup)

The three operations are defined as follows:
- read: copy one sector (512B) from the "disk" to the address specified
- write: copy one sector (512B) from the address to the disk sector specified
- setup: create the disk status structure at the specified address

The status structure contains information about:
 - disk size
 - last operation error

The setup operation must be called before any other operation and may be called
only once. The status structure will be updated by the host in place.

| port | direction | description                                               |
| ---- | --------- | --------------------------------------------------------- |
| 0x20 |    out    | sets the offset of the disk (dword)                       |
| 0x21 |    out    | sets the offset in the guest memory for DMA (**dword**)   |
| 0x22 |    out    | operation code (byte)                                     |
