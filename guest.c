#include "guest_io.h"

const char LOREM_IPSUM[HDD_SECTOR_SIZE] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam eget ex "
    "in nibh convallis ultrices non non turpis. Pellentesque ultrices at mi "
    "nec porta. Phasellus malesuada mi sapien, ac varius ligula sodales eu. "
    "Praesent bibendum nisi ut nisl volutpat, non vulputate erat ornare. Proin "
    "nec dignissim dolor. Donec fringilla nisi in pulvinar facilisis. Aenean a "
    "tellus ut lorem sollicitudin aliquet id sit amet tortor. In non semper "
    "orci. Etiam suscipit, lacus at facilisis gravida, ipsum lectus aliquet "
    "sapien.";

int test_lorem_ipsum(volatile struct hdd_status *h, int offset, int size) {
    char *buf = malloc(size);
    int res;

    puts("writing to disk: off=");
    puti(offset);
    puts(", size=");
    puti(size);
    puts("\n");

    for (int i = 0; i < size; i++) {
        buf[i] = LOREM_IPSUM[i % HDD_SECTOR_SIZE];
    }

    res = hdd_write(h, offset, buf, size);
    if (res < 0) {
        puts("error writing to disk: ");
        puti(res);
        puts("\n");
        return res;
    }
    puts("write complete, bytes written: ");
    puti(res);
    puts("\n");

    memset(buf, 0, size);

    puts("reading from disk: off=");
    puti(offset);
    puts(", size=");
    puti(size);
    puts("\n");

    res = hdd_read(h, offset, buf, size);
    if (res < 0) {
        puts("error reading from disk: ");
        puti(res);
        puts("\n");
        return res;
    }
    puts("read complete, bytes read: ");
    puti(res);
    puts("\n");

    puts("peek buf[:11]: ");
    for (int i = 0; i < 11; i++) putc(buf[i]);
    puts("\n");

    puts("validating read... ");

    res = 0;
    for (int i = 0; i < size; i++) {
        if (buf[i] != LOREM_IPSUM[i % HDD_SECTOR_SIZE]) {
            res = 1;
            puts(
                "ERROR\n"
                "Offset: ");
            puti(i);
            puts("\nExpected: ");
            putc(LOREM_IPSUM[i % HDD_SECTOR_SIZE]);
            puts("\nFound: ");
            putc(buf[i]);
            puts("\n");
            break;
        }
    }
    if (res == 0) {
        puts("OK\n");
    }
    return res;
}

#define EXPECT(expected, actual)      \
    do {                              \
        puts(__func__);               \
        if ((expected) != (actual)) { \
            puts(" ERROR ");          \
            puti(actual);             \
            puts(" != ");             \
            puti(expected);           \
            puts("\n\n");             \
        } else {                      \
            puts(" OK\n\n");          \
        }                             \
    } while (0);

void test_lorem_ipsum_first_sector_aligned(volatile struct hdd_status *h) {
    int res = test_lorem_ipsum(h, 0, HDD_SECTOR_SIZE);
    EXPECT(0, res);
}

void test_lorem_ipsum_first_sector_part(volatile struct hdd_status *h) {
    int res = test_lorem_ipsum(h, 0, 50);
    EXPECT(0, res);
}

void test_lorem_ipsum_second_sector_aligned(volatile struct hdd_status *h) {
    int res = test_lorem_ipsum(h, HDD_SECTOR_SIZE, HDD_SECTOR_SIZE);
    EXPECT(0, res);
}

void test_lorem_ipsum_second_sector_part(volatile struct hdd_status *h) {
    int res = test_lorem_ipsum(h, HDD_SECTOR_SIZE, 50);
    EXPECT(0, res);
}

int test_lorem_ipsum_two_sectors_aligned(volatile struct hdd_status *h) {
    int res = test_lorem_ipsum(h, 0, 2 * HDD_SECTOR_SIZE);
    EXPECT(0, res);
}

int test_lorem_ipsum_two_sectors_misaligned(volatile struct hdd_status *h) {
    int res = test_lorem_ipsum(h, 50, 2 * HDD_SECTOR_SIZE);
    EXPECT(0, res);
}

int test_lorem_ipsum_all_sectors_aligned(volatile struct hdd_status *h) {
    int res = test_lorem_ipsum(h, 0, h->size);
    EXPECT(0, res);
}

int test_lorem_ipsum_bad_sector(volatile struct hdd_status *h) {
    int res = test_lorem_ipsum(h, h->size, h->size + HDD_SECTOR_SIZE);
    EXPECT(-EINVAL, res);
}

void main() {
    volatile struct hdd_status h;
    int res;

    puts("Hello world!\n");

    res = hdd_setup(&h);
    if (res) {
        puts("ERROR setting up disk!\n");
        return;
    }
    puts("Disk set up!\n");

    test_lorem_ipsum_first_sector_aligned(&h);
    test_lorem_ipsum_first_sector_part(&h);
    test_lorem_ipsum_second_sector_aligned(&h);
    test_lorem_ipsum_second_sector_part(&h);
    test_lorem_ipsum_two_sectors_aligned(&h);
    test_lorem_ipsum_two_sectors_misaligned(&h);
    test_lorem_ipsum_all_sectors_aligned(&h);
    test_lorem_ipsum_bad_sector(&h);
}
