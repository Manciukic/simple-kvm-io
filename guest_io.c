#include "guest_io.h"

#define min(x, y) ((x) < (y) ? (x) : (y))

void *heap_p = (void*) (1<<20); // start heap at 1MB and grow up

static void outb(const char b, const ioport port)
{
    asm ("outb %0, %1"
        : // no output operand
        : "r"(b), "r"(port)
        );
}

static void outl(const int l, const ioport port)
{
    asm ("outl %0, %1"
        : // no output operand
        : "r"(l), "r"(port)
        );
}

void memcpy(char *dest, const char *src, unsigned size)
{
    for (unsigned i = 0; i < size; i++) {
        dest[i] = src[i];
    }
}

void memset(char *buf, char c, unsigned size)
{
    for (unsigned i = 0; i < size; i++) {
        buf[i] = c;
    }
}

void *malloc(unsigned size)
{
    void *res = heap_p;

    heap_p += size;
    return res;
}

void putc(char c)
{
    outb(c, SERIAL_PORT);
}

void puts(const char *s)
{
    while (*s != '\0')
        putc(*(s++));
}

void puti(int i)
{
    int x = 10, d;

    if (i < 0) {
        putc('-');
        i *= -1;
    }

    while (i > x) {
        x *= 10;
    }

    x /= 10;
    while (x) {
        d = i / x;
        i -= d*x;
        x /= 10;

        putc('0'+d);
    }
}

#define OFF32(x) ((int) (((unsigned long) (x)) & 0xffffffff))

int hdd_setup(volatile struct hdd_status *h)
{
    h->err = 1;
    outl(OFF32(h), HDD_DMA_ADDR_PORT);
    outb(HDD_CMD_SETUP, HDD_CMD_PORT);
    return h->err; // device will set to 0 when correctly setup
}

static int hdd_read_sector_full(volatile struct hdd_status *h, int sector,
                                char *buf)
{
    outl(sector, HDD_SECTOR_PORT);
    outl(OFF32(buf), HDD_DMA_ADDR_PORT);
    outb(HDD_CMD_READ, HDD_CMD_PORT);
    if (h->err)
        return -h->err;
    else
        return HDD_SECTOR_SIZE;
}

static int hdd_read_sector_part(volatile struct hdd_status *h, int sector,
                                int sector_off, char *buf, unsigned size)
{
    char sector_buf[HDD_SECTOR_SIZE];
    int res;

    res = hdd_read_sector_full(h, sector, sector_buf);
    if (res < 0)
        return res;

    memcpy(buf, sector_buf+sector_off, size);
    return size;
}

int hdd_read(volatile struct hdd_status *h, int offset, char *buf,
            unsigned size)
{
    unsigned bytes_read = 0, read_size;
    int res, sector, sector_off;

    while (bytes_read < size) {
        sector = offset / HDD_SECTOR_SIZE;
        sector_off = offset - sector * HDD_SECTOR_SIZE;
        read_size = min(size - bytes_read, HDD_SECTOR_SIZE - sector_off);

        if (sector_off == 0 && read_size == HDD_SECTOR_SIZE) {
            res = hdd_read_sector_full(h, sector, buf);
        } else {
            res = hdd_read_sector_part(h, sector, sector_off, buf, read_size);
        }
        if (res < 0) {
            return res;
        }
        bytes_read += read_size;
        offset += read_size;
        buf += read_size;
    }

    return bytes_read;
}

static int hdd_write_sector_full(volatile struct hdd_status *h, int sector,
                                const char *buf)
{
    outl(sector, HDD_SECTOR_PORT);
    outl(OFF32(buf), HDD_DMA_ADDR_PORT);
    outb(HDD_CMD_WRITE, HDD_CMD_PORT);
    if (h->err)
        return -h->err;
    else
        return HDD_SECTOR_SIZE;
}

static int hdd_write_sector_part(volatile struct hdd_status *h, int sector,
                                int sector_off, const char *buf, unsigned size)
{
    char sector_buf[HDD_SECTOR_SIZE];
    int res;

    res = hdd_read_sector_full(h, sector, sector_buf);
    if (res < 0)
        return res;

    memcpy(sector_buf+sector_off, buf, size);

    res = hdd_write_sector_full(h, sector, sector_buf);
    if (res < 0)
        return res;

    return size;
}


int hdd_write(volatile struct hdd_status *h, int offset, const char *buf,
            unsigned size)
{
    unsigned bytes_written = 0, write_size;
    int res, sector, sector_off;

    while (bytes_written < size) {
        sector = offset / HDD_SECTOR_SIZE;
        sector_off = offset - sector * HDD_SECTOR_SIZE;
        write_size = min(size - bytes_written, HDD_SECTOR_SIZE - sector_off);

        if (sector_off == 0 && write_size == HDD_SECTOR_SIZE) {
            res = hdd_write_sector_full(h, sector, buf);
        } else {
            res = hdd_write_sector_part(h, sector, sector_off, buf,
                                        write_size);
        }
        if (res < 0) {
            return res;
        }
        bytes_written += write_size;
        offset += write_size;
        buf += write_size;
    }

    return bytes_written;
}
