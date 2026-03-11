#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define GPIO_BASE 0xFE200000
#define BLOCK_SIZE 4096

volatile unsigned int *gpio;

int main()
{
    int fd;
    void *gpio_map;

    printf("INFO: Starting GPIO register level program\n");

    printf("INFO: Opening /dev/mem...\n");
    fd = open("/dev/mem", O_RDWR | O_SYNC);

    if(fd < 0)
    {
        printf("ERROR: Cannot open /dev/mem. Run with sudo.\n");
        return -1;
    }

    printf("INFO: Mapping GPIO registers...\n");

    gpio_map = mmap(
        NULL,
        BLOCK_SIZE,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        GPIO_BASE
    );

    if(gpio_map == MAP_FAILED)
    {
        printf("ERROR: mmap failed\n");
        close(fd);
        return -1;
    }

    gpio = (volatile unsigned int*)gpio_map;

    printf("INFO: GPIO mapped successfully\n");
    printf("INFO: Configuring GPIO18 as OUTPUT\n");

    // Clear function bits
    gpio[1] &= ~(7 << 24);

    // Set output mode
    gpio[1] |= (1 << 24);

    printf("INFO: GPIO18 configured as OUTPUT\n");
    printf("INFO: Starting LED blink loop\n");

    while(1)
    {
        printf("INFO: LED ON\n");
        gpio[7] = (1 << 18);
        sleep(1);

        printf("INFO: LED OFF\n");
        gpio[10] = (1 << 18);
        sleep(1);
    }

    return 0;
