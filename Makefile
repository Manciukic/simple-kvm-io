CFLAGS = -Wall -Wextra -Werror -O0 -g

test: test.o payload.o
	$(CC) $^ -o $@

payload.o: payload.ld guest.o
	$(LD) -T $< -o $@

guest.o: guest.s
	$(CC) -c guest.s -o $@

clean:
	$(RM) test *.o *.img

