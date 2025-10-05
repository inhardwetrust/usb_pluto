

#include "adi_test.h"

#include "xil_types.h"    // UINTPTR, u8/u32
#include "xil_cache.h"    // Xil_DCacheFlushRange
#include <string.h>       // memset


#include "no_os_spi.h"
#include "xilinx_spi.h"

/* === GPIO context === */
static XGpioPs *g_gpio = NULL;
static uint32_t g_gpio_pin_rst = 0xFFFFFFFF;   // invalid pin -> GPIO не активен

static struct no_os_spi_desc *spi;

void adi_set_gpio(XGpioPs *gpio, uint32_t pin) {
	g_gpio = gpio;
	g_gpio_pin_rst = pin;
}

//XGpioPs_WritePin(g_gpio, g_gpio_pin, 0);

int reset_init_and_pulse(void)
{


    // аппаратный ресет: 0 → 10ms → 1
    XGpioPs_WritePin(g_gpio, g_gpio_pin_rst, 0);
    usleep(100000);
    XGpioPs_WritePin(g_gpio, g_gpio_pin_rst, 1);
    usleep(100000);
    return XST_SUCCESS;
}

int spi_init_ps(void)
{
    struct xil_spi_init_param xil_spi = {
        .type  = SPI_PS,
        .flags = 0
    };
    struct no_os_spi_init_param ip = {
        .device_id    = SPI_DEVICE_ID,
        .max_speed_hz = SPI_HZ,
        .mode         = SPI_MODE,
        .bit_order    = NO_OS_SPI_BIT_ORDER_MSB_FIRST,
        .chip_select  = SPI_CS,
        .platform_ops = &xil_spi_ops,
        .extra        = &xil_spi,
    };

    int ret = no_os_spi_init(&spi, &ip);
    if (ret) return ret;

    return no_os_spi_init(&spi, &ip);
}

int ad936x_reg_write(uint16_t addr, uint8_t val)
{
    uint8_t tx[3];
    tx[0] = (addr >> 8) & 0x7F;    // W: RW=0
    tx[1] = addr & 0xFF;
    tx[2] = val;
    return no_os_spi_write_and_read(spi, tx, sizeof(tx));
}

int ad936x_reg_read(uint16_t addr, uint8_t *val)
{
    uint8_t buf[3];
    buf[0] = ((addr >> 8) & 0x7F) | 0x80;  // R: RW=1
    buf[1] = addr & 0xFF;
    buf[2] = 0x00;
    int st = no_os_spi_write_and_read(spi, buf, sizeof(buf));
    if (st) return st;
    *val = buf[2];
    return 0;
}

void my_spi() {

	//reset_init_and_pulse();
	//spi_init_ps();



	//uint8_t wr = 0xA5, rd = 0;
	//int32_t result=ad936x_reg_write(REG_SCRATCH, wr);

	uint8_t rd = 0;
	int32_t result_rd= ad936x_reg_read(REG_PRODUCT_ID, &rd);
	//0x43 (AD9363) или 0x41 (AD9361).

}


//// adi version
#include "ad9361.h"

struct no_os_spi_desc *g_spi = NULL;
struct ad9361_rf_phy  g_phy  = {0};

int adi_spi_prepare(void)
{
    struct xil_spi_init_param xil = {
        .type  = SPI_PS,
        .flags = 0
    };
    struct no_os_spi_init_param ip = {
        .device_id    = SPI_DEVICE_ID,
        .chip_select  = SPI_CS,
        .max_speed_hz = SPI_HZ,
        .mode         = SPI_MODE,
        .bit_order    = NO_OS_SPI_BIT_ORDER_MSB_FIRST,  /// NO_OS_SPI_BIT_ORDER_LSB_FIRST NO_OS_SPI_BIT_ORDER_MSB_FIRST
        .platform_ops = &xil_spi_ops,
        .extra        = &xil
    };
    return no_os_spi_init(&g_spi, &ip);
}

void my_spi_adi() {
	int32_t v = ad9361_spi_read(g_spi, REG_PRODUCT_ID);
}


