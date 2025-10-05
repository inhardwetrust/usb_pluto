// adi_test.h

#include <stdint.h>
#include "xgpiops.h"

#define SPI_DEVICE_ID    0             // PS SPI0
#define SPI_CS           0             // CSN0
#define SPI_HZ           1000000
#define SPI_MODE         NO_OS_SPI_MODE_1 //NO_OS_SPI_MODE_0   NO_OS_SPI_MODE_3

// Подставь адреса из даташита/no-OS для AD9361/AD9363:
#define REG_SCRATCH      0x000A        // удобен для RW-проверки (если другой — замени)
#define REG_ID           0x0037        // пример: «product ID»/revision (если другой — замени)

#define REG_PRODUCT_ID	0x037





void adi_set_gpio(XGpioPs *gpio, uint32_t pin);
int reset_init_and_pulse(void);
int spi_init_ps(void);
int ad936x_reg_write(uint16_t addr, uint8_t val);
int ad936x_reg_read(uint16_t addr, uint8_t *val);
void my_spi();
