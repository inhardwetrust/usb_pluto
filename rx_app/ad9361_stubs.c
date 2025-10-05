#include "ad9361.h"

int32_t ad9361_hdl_loopback(struct ad9361_rf_phy *phy, bool enable)
{
    /* без HDL просто «ОК» */
    return 0;
}

int32_t ad9361_dig_tune(struct ad9361_rf_phy *phy, uint32_t max_freq,
			enum dig_tune_flags flags)
{
    /* пропускаем автодоворот интерфейса */
    return 0;
}
