SECTIONS
{
        .payload64 0 : {
                guest = .;
                guest.o
                guest_end = .;
        }
}
