CFLAGS = -Wall -Wextra -Werror -O0 -g

test: test.o guest.flat
	$(CC) test.o -o $@

guest.flat: payload.o
	objcopy -O binary $^ $@

payload.o: payload.ld guest.o guest_load.o
	$(LD) -T $< -o $@

guest_load.o: guest_load.s
	$(CC) -c $^ -o $@

guest.o: guest.c
	$(CC) -nostdinc -fno-builtin -c $^ -o $@


clean:
	$(RM) test *.o *.img *.flat

