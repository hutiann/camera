#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <mtd/mtd-user.h>

int main()
{
    mtd_info_t mtd_info;           // the MTD structure
    erase_info_t ei;               // the erase block structure
    int i;

    unsigned char data[19] = { 0xDE, 0xAE, 0xBE, 0xEF,  // our data to write
                               0xDC, 0xAC, 0xBC, 0xEC,
                               0xDB, 0xAB, 0xBB, 0xEB,
                               0xDA, 0xAA, 0xBA, 0xEA,
                               0xD0, 0xA0, 0xB0};
    unsigned char read_buf[19] = {0x00};                // empty array for reading

    int fd = open("/dev/mtd10", O_RDWR); // open the mtd device for reading and writing
                                        
    ioctl(fd, MEMGETINFO, &mtd_info);   // get the device info

    // dump it for a sanity check, should match what's in /proc/mtd

    printf("MTD Type: %x\nMTD total size: %x bytes\nMTD erase size: %x bytes\n",
         mtd_info.type, mtd_info.size, mtd_info.erasesize);

    ei.length = mtd_info.erasesize;   //set the erase block size
    for(ei.start = 0; ei.start < mtd_info.size; ei.start += ei.length)
    {
        ioctl(fd, MEMUNLOCK, &ei);
        // printf("Eraseing Block %#x\n", ei.start); // show the blocks erasing
                                                  // warning, this prints a lot!
        ioctl(fd, MEMERASE, &ei);
    }    

    lseek(fd, 0, SEEK_SET);               // go to the first block
    read(fd, read_buf, sizeof(read_buf)); // read 20 bytes

    // sanity check, should be all 0xFF if erase worked

    for(i = 0; i<20; i++)
        printf("buf[%d] = 0x%02x\n", i, (unsigned int)read_buf[i]);

    lseek(fd, 0, SEEK_SET);        // go back to first block's start
    write(fd, data, sizeof(data)); // write our message

    lseek(fd, 0, SEEK_SET);              // go back to first block's start
    read(fd, read_buf, sizeof(read_buf));// read the data

    // sanity check, now you see the message we wrote!    

    for(i = 0; i<20; i++)
         printf("buf[%d] = 0x%02x\n", i, (unsigned int)read_buf[i]);

    close(fd);
    return 0;
}
