# The date and time the ROM was built, in the form C's ctime() gives with a
# DOS line ending. main.c reads the whole segment, padding included, into
# g_RomBuildDate, and menu.c stores it in the save data.

.section .data

    .ascii "Tue Jun 15 18:36:11 1999\r\n"
    .balign 16
