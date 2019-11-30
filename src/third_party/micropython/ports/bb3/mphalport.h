// empty file

#include "py/obj.h"

#define mp_hal_stdin_rx_chr() (0)
void mp_hal_stdout_tx_strn(const char *str, size_t len);

void mp_hal_delay_ms(mp_uint_t ms);
void mp_hal_delay_us(mp_uint_t us);
mp_uint_t mp_hal_ticks_ms(void);
mp_uint_t mp_hal_ticks_us(void);
mp_uint_t mp_hal_ticks_cpu(void);
