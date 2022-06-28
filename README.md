# Simple KVM IO

## Specification

### Serial port

The serial port makes it possible to send messages from the guest to the host.
These messages will be print to stdout by the vmm.

| port | direction | description                                               |
| ---- | --------- | --------------------------------------------------------- |
| 0x10 |    out    | write the payload to the serial port                      |


