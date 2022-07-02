#include "guest_io.h"
#include "io.h"

void outb(const char c, const ioport port) {
    asm ("outb %0, %1"
        : // no output operand
        : "r"(c), "r"(port)
        );
}

void puts(const char *s) {
    while (*s != '\0') {
        outb(*s, SERIAL_PORT);
        s++;
    }
}
