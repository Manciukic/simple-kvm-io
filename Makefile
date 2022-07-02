CFLAGS = -Wall -Wextra -Werror -O0 -g
GUEST_CFLAGS = -nostdinc -fno-builtin

all: test guest.flat

test: test.o host_io.o
	$(CC) $^ -o $@

guest.flat: payload.o
	objcopy -O binary $^ $@

payload.o: payload.ld guest.o guest_load.o guest_io.o
	$(LD) -T $< -o $@

guest_load.o: guest_load.s
	$(CC) -c $^ -o $@

guest_io.o: guest_io.c
	$(CC) $(GUEST_CFLAGS) -c $^ -o $@

guest.o: guest.c
	$(CC) $(GUEST_CFLAGS) -c $^ -o $@

clean:
	$(RM) test *.o *.img *.flat

disk:
	rm -f disk.raw
	dd if=/dev/zero of=disk.raw bs=512 count=16

run: clean disk all
	./test
