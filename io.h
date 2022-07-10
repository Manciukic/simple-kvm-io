#define MMIO_ADDR 0x200000

#define SERIAL_PORT 0x10

#define HDD_SECTOR_PORT 0x20
#define HDD_DMA_ADDR_PORT 0x21
#define HDD_CMD_PORT 0x22

#define HDD_SECTOR_SIZE 512

#define HDD_CMD_READ 0
#define HDD_CMD_WRITE 1
#define HDD_CMD_SETUP 2

#define EINVAL 22
#define EFAULT 14

struct hdd_status {
    unsigned long long size;
    int err;
};
