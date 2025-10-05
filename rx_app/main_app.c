#include <inttypes.h>
#include "ad9361_api.h"     // Либо ad9361.h (зависит от твоей версии no-OS)
#include "no_os_spi.h"
#include "no_os_gpio.h"
#include "no_os_delay.h"

#include "xilinx_spi.h"
#include "xilinx_gpio.h"





#define GPIO_RESETB    54           // твой EMIO GPIO номер для RESETB (пример!)
#define SPI_DEVICE_ID  0            // PS SPI0 (1 — если используешь SPI1)
#define SPI_CS         0            // Chip Select (как разведено)
#define REF_CLK_HZ     40000000UL   // 40 МГц на AD9363 (как на Pluto)
#define RX_LO_HZ       100000000UL  // Задай свою частоту приёма
#define RX_BW_HZ       2000000UL    // Аналоговая полоса приёма
#define RX_FS_HZ       4000000UL    // Частота дискретизации на выходе AD9363


int main_app(void);

int main_app (void)
{
    // --- Xilinx platform ops (PS SPI + PS GPIO)
    struct xil_spi_init_param xil_spi = {
        .type  = SPI_PS,
        .flags = 0
    };
    struct xil_gpio_init_param xil_gpio = {
        .type = GPIO_PS,
        .device_id = 0
    };

    struct no_os_spi_init_param spi_ip = {
        .device_id    = SPI_DEVICE_ID,
        .max_speed_hz = 10000000,             // старт на 10 МГц
        .mode         = NO_OS_SPI_MODE_0,
        .bit_order    = NO_OS_SPI_BIT_ORDER_MSB_FIRST,
        .chip_select  = SPI_CS,
        .platform_ops = &xil_spi_ops,
        .extra        = &xil_spi
    };

    struct no_os_gpio_init_param gpio_rst_ip = {
        .number       = GPIO_RESETB,
        .platform_ops = &xil_gpio_ops,
        .extra        = &xil_gpio
    };

    struct no_os_gpio_desc *gpio_rst = NULL;
    no_os_gpio_get(&gpio_rst, &gpio_rst_ip);
    no_os_gpio_direction_output(gpio_rst, 0);
    no_os_mdelay(10);
    no_os_gpio_set_value(gpio_rst, 1);
    no_os_mdelay(10);

    // --- Минимальные init-параметры AD9363
    AD9361_InitParam p = {0};

    p.dev_sel = ID_AD9363A;                 // именно 9363
    p.reference_clk_rate = REF_CLK_HZ;

    // ENSM управляется SPI (пины ENABLE/TXNRX не используем)
    p.ensm_enable_pin_pulse_mode_enable = 0;
    p.ensm_enable_txnrx_control_enable  = 0;

    // LO и частоты
    p.rx_synthesizer_frequency_hz = RX_LO_HZ;
    p.tx_synthesizer_frequency_hz = RX_LO_HZ; // можно не использовать TX, но поле нужно
    // Цепочка тактирования (пример под ~RX_FS_HZ=4e6)
    // Для старых API тут задаются частоты BBPLL и делителей, но проще:
    // после init выставим через API sampling/bandwidth, драйвер пересчитает цепочку.

    // Полоса тракта
    p.rf_rx_bandwidth_hz = RX_BW_HZ;
    p.rf_tx_bandwidth_hz = RX_BW_HZ;

    // LVDS DDR интерфейс (как у Pluto)
    p.lvds_mode_enable = 1;
    p.lvds_rx_onchip_termination_enable = 1;
    p.rx_frame_pulse_mode_enable = 1;   // FRAME=пульс на posedge выборки
    p.pp_rx_swap_enable = 1;            // как в реф-дизайне Pluto

    // Режимы усиления — начнём со slow-attack
    p.gc_rx1_mode = 2;  // 0=manual, 1=fast, 2=slow
    p.gc_rx2_mode = 2;

    // Привязка SPI/GPIO в структуре init
    p.gpio_resetb.number   = GPIO_RESETB;
    p.gpio_resetb.platform_ops = &xil_gpio_ops;
    p.gpio_resetb.extra    = &xil_gpio;

    p.spi_param.device_id    = SPI_DEVICE_ID;
    p.spi_param.chip_select  = SPI_CS;
    p.spi_param.mode         = NO_OS_SPI_MODE_0;
    p.spi_param.platform_ops = &xil_spi_ops;
    p.spi_param.extra        = &xil_spi;

    struct ad9361_rf_phy *phy = NULL;

    // --- Инициализация чипа (PLL, делители, калибровки, ENSM)
    if (ad9361_init(&phy, &p) != 0) {
        // ошибка
        while (1) { }
    }

    // Задать частоту дискретизации и полосу «по-человечески»
    ad9361_set_rx_sampling_freq(phy, RX_FS_HZ);
    ad9361_set_rx_rf_bandwidth(phy, RX_BW_HZ);
    ad9361_set_rx_lo_freq(phy, RX_LO_HZ);

    // Перевод в RX (если вдруг не там)
    ad9361_set_en_state_machine_mode(phy, ENSM_MODE_RX);

    // Дальше твой цикл: PL уже получает rx_clk/frame/data
    //while (1) {
        // stream via AXI DMA → USB
    //}

    // (не дойдём сюда)
    //ad9361_remove(phy);
    return 0;
}
