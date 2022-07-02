#define ioport unsigned short int
#define addr void*

#define NULL ((addr) 0)

const ioport SERIAL_PORT = 0x10;

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

void main() {
    puts("Hello world!\n");
}

