#include "io.h"

#define ioport unsigned short int
#define addr void *

#define NULL ((addr)0)

extern void memcpy(char *dest, const char *src, unsigned size);
extern void memset(char *buf, char c, unsigned size);
extern void *malloc(unsigned size);

extern void putc(char c);
extern void puts(const char *s);
extern void puti(int i);

extern int hdd_setup(volatile struct hdd_status *h);
extern int hdd_read(volatile struct hdd_status *h, int offset, char *buf,
                    unsigned size);
extern int hdd_write(volatile struct hdd_status *h, int offset, const char *buf,
                     unsigned size);
