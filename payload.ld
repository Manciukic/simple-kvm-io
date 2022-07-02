SECTIONS
{
        .payload64 0 : {
                guest_load.o
                guest.o
                guest_io.o
        }
}
