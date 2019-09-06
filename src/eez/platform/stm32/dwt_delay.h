#pragma once

#include "stm32f7xx_hal.h"

#define DWT_CYCCNT ((volatile unsigned int *)0xE0001004)

void DWT_Delay_Init(void);

extern uint32_t g_cyclesPerMicrosecond;

inline void DWT_Delay_us(volatile uint32_t microseconds) {
	uint32_t clkCycleStart = *DWT_CYCCNT;
	uint32_t clkCycleEnd = clkCycleStart + microseconds * g_cyclesPerMicrosecond;
	while (*DWT_CYCCNT < clkCycleEnd);
}

inline uint32_t DWT_micros() {
	return *DWT_CYCCNT / g_cyclesPerMicrosecond;
}
