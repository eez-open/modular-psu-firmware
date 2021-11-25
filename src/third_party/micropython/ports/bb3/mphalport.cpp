/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2013, 2014 Damien P. George and 2017, 2018 Rami Ali
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#if defined(EEZ_PLATFORM_SIMULATOR)
#include <stdio.h>
#endif

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4200)
#endif

extern "C" {
#include "mphalport.h"
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

#include <eez/debug.h>
#include <eez/os.h>

extern"C" void mp_hal_stdout_tx_strn(const char *str, size_t len) {
    DebugTrace("%.*s", len, str);
}

extern"C" void mp_hal_delay_ms(mp_uint_t ms) {
    osDelay(ms);
}

extern"C" void mp_hal_delay_us(mp_uint_t us) {
    osDelay((us + 999) / 1000);
}

extern"C" mp_uint_t mp_hal_ticks_us(void) {
    return osKernelGetTickCount() * 1000;
}

extern"C" mp_uint_t mp_hal_ticks_ms(void) {
    return osKernelGetTickCount();
}

extern"C" mp_uint_t mp_hal_ticks_cpu(void) {
    return osKernelGetTickCount();
}
