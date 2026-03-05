#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sys/select.h>

#define UART_DEVICE "/dev/serial0"
#define BUFFER_SIZE 256

int main()
{
    int uart_fd;
    struct termios options;

    char rx_buffer[BUFFER_SIZE];
    char line_buffer[BUFFER_SIZE];
    int line_index = 0;

    uart_fd = open(UART_DEVICE, O_RDWR | O_NOCTTY);

    if (uart_fd < 0)
    {
        perror("UART open failed");
        return -1;
    }

    printf("UART opened successfully\n");

    tcgetattr(uart_fd, &options);

    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);

    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;

    options.c_lflag = 0;
    options.c_iflag = 0;
    options.c_oflag = 0;

    options.c_cc[VMIN] = 1;
    options.c_cc[VTIME] = 0;

    tcsetattr(uart_fd, TCSANOW, &options);

    printf("UART configured (115200 8N1)\n\n");

    while (1)
    {
        fd_set readfds;
        FD_ZERO(&readfds);

        FD_SET(uart_fd, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        int maxfd = uart_fd > STDIN_FILENO ? uart_fd : STDIN_FILENO;

        select(maxfd + 1, &readfds, NULL, NULL, NULL);

        /* Data from PC */
        if (FD_ISSET(uart_fd, &readfds))
        {
            int n = read(uart_fd, rx_buffer, BUFFER_SIZE);

            for (int i = 0; i < n; i++)
            {
                char c = rx_buffer[i];

                if (c == '\n')
                {
                    line_buffer[line_index] = '\0';
                    printf("Message from PC  : %s\n", line_buffer);
                    line_index = 0;
                }
                else
                {
                    if (line_index < BUFFER_SIZE - 1)
                        line_buffer[line_index++] = c;
                }
            }
        }

        /* Data from Raspberry Pi keyboard */
        if (FD_ISSET(STDIN_FILENO, &readfds))
        {
            int n = read(STDIN_FILENO, rx_buffer, BUFFER_SIZE);

            if (n > 0)
            {
                write(uart_fd, rx_buffer, n);
                printf("Message from RPi : %.*s", n, rx_buffer);
            }
        }
    }

    close(uart_fd);
    return 0;
}
