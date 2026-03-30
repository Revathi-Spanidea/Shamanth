#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

#define I2C_DEVICE "/dev/i2c-1"
#define LIS2DU12_ADDR 0x18

#define WHO_AM_I_REG 0x0F
#define CTRL1_REG    0x10
#define CTRL3_REG    0x12
#define CTRL4_REG    0x13
#define CTRL5_REG    0x14
#define OUTX_L_REG   0x28

int i2c_fd;

/* Write register */
int i2c_write_reg(uint8_t reg, uint8_t data)
{
    uint8_t buffer[2];
    buffer[0] = reg;
    buffer[1] = data;

    if (write(i2c_fd, buffer, 2) != 2)
    {
        perror("I2C write failed");
        return -1;
    }
    return 0;
}

/* Read multiple bytes */
int i2c_read_regs(uint8_t reg, uint8_t *buffer, uint8_t len)
{
    if (write(i2c_fd, &reg, 1) != 1)
    {
        perror("I2C write register failed");
        return -1;
    }

    if (read(i2c_fd, buffer, len) != len)
    {
        perror("I2C read failed");
        return -1;
    }

    return 0;
}

/* Sensor initialization */
void LIS2DU12_Init()
{
    usleep(50000);

    i2c_write_reg(CTRL1_REG, 0x37);
    i2c_write_reg(CTRL3_REG, 0x20);
    i2c_write_reg(CTRL4_REG, 0x08);
    i2c_write_reg(CTRL5_REG, 0x80);

    usleep(50000);

    printf("LIS2DU12 Init done\n");
}

/* Read acceleration */
void LIS2DU12_ReadAccel()
{
    uint8_t buffer[6];

    if (i2c_read_regs(OUTX_L_REG | 0x80, buffer, 6) < 0)
        return;

    int16_t raw_x = (buffer[1] << 8) | buffer[0];
    int16_t raw_y = (buffer[3] << 8) | buffer[2];
    int16_t raw_z = (buffer[5] << 8) | buffer[4];

    float ax = raw_x * 0.061f / 1000.0f;
    float ay = raw_y * 0.061f / 1000.0f;
    float az = raw_z * 0.061f / 1000.0f;

    printf("X: %.3f g  Y: %.3f g  Z: %.3f g\n", ax, ay, az);
}

int main()
{
    printf("LIS2DU12 Sensor Interfacing via I2C (RPi)\n");

    i2c_fd = open(I2C_DEVICE, O_RDWR);
    if (i2c_fd < 0)
    {
        perror("Failed to open I2C device");
        return -1;
    }

    if (ioctl(i2c_fd, I2C_SLAVE, LIS2DU12_ADDR) < 0)
    {
        perror("Failed to select I2C device");
        return -1;
    }

    LIS2DU12_Init();

    while (1)
    {
        LIS2DU12_ReadAccel();
        usleep(500000);
    }

    close(i2c_fd);
    return 0;
}
