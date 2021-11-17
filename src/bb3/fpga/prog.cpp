/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if EEZ_PLATFORM_STM32
#include <main.h>
#include <eez/platform/stm32/spi.h>
#endif

#include <bb3/psu/psu.h>
#include <bb3/psu/scpi/psu.h>
#include <bb3/psu/gui/psu.h>

#include <eez/libs/sd_fat/sd_fat.h>

#include <bb3/fpga/prog.h>

namespace eez {
namespace fpga {

#if defined(EEZ_PLATFORM_SIMULATOR)

#define DOUT2_GPIO_Port 0
#define DOUT2_Pin 0
#define GPIOB 0
#define GPIO_PIN_14 0
#define GPIOI 0
#define GPIO_PIN_3 0
#define GPIO_PIN_1 0

typedef uint32_t GPIO_TypeDef;

enum GPIO_PinState {
    GPIO_PIN_RESET = 0,
    GPIO_PIN_SET
};

void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint32_t pin, GPIO_PinState state) {
}

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin) {
    return GPIO_PIN_SET;
}

#endif

struct Pin {
    Pin(GPIO_TypeDef *port, uint32_t pin) : m_port(port), m_pin(pin) {
    }

    void on() {
        HAL_GPIO_WritePin(m_port, m_pin, GPIO_PIN_SET);
    }

    void off() {
        HAL_GPIO_WritePin(m_port, m_pin, GPIO_PIN_RESET);
    }

    GPIO_PinState value() {
        return HAL_GPIO_ReadPin(m_port, m_pin);
    }

    GPIO_TypeDef *m_port;
    uint32_t m_pin;
};

Pin tms(DOUT2_GPIO_Port, DOUT2_Pin);
Pin tdi(GPIOB, GPIO_PIN_14);
Pin tdo(GPIOI, GPIO_PIN_3);
Pin tck(GPIOI, GPIO_PIN_1);

void bitbang_jtag_on() {
    // led=Pin(gpio_led,Pin.OUT)
    // tms=Pin(gpio_tms,Pin.OUT)
    // tck=Pin(gpio_tck,Pin.OUT)
    // tdi=Pin(gpio_tdi,Pin.OUT)
    // tdo=Pin(gpio_tdo,Pin.IN)
}

void bitbang_jtag_off() {
    // led=Pin(gpio_led,Pin.IN)
    // tms=Pin(gpio_tms,Pin.IN)
    // tck=Pin(gpio_tck,Pin.IN)
    // tdi=Pin(gpio_tdi,Pin.IN)
    // tdo=Pin(gpio_tdo,Pin.IN)
    // a = led.value()
    // a = tms.value()
    // a = tck.value()
    // a = tdo.value()
    // a = tdi.value()
    // del led
    // del tms
    // del tck
    // del tdi
    // del tdo
}

void spi_jtag_on() {
    // global hwspi,swspi
    // hwspi=SPI(spi_channel, baudrate=spi_freq, polarity=1, phase=1, bits=8, firstbit=SPI.MSB, sck=Pin(gpio_tck), mosi=Pin(gpio_tdi), miso=Pin(gpio_tdo))
    // swspi=SPI(-1, baudrate=spi_freq, polarity=1, phase=1, bits=8, firstbit=SPI.MSB, sck=Pin(gpio_tck), mosi=Pin(gpio_tdi), miso=Pin(gpio_tdo))
}

void spi_jtag_off() {
    // hwspi.deinit()
    // del hwspi

    // swspi.deinit()
    // del swspi
}

void send_tms(int val) {
    if (val) {
        tms.on();
    } else {
        tms.off();
    }
    tck.off();
    tck.on();
}

void send_tms0111() {
    send_tms(0); // # -> pause DR
    send_tms(1); // # -> exit 2 DR
    send_tms(1); // # -> update DR
    send_tms(1); // # -> select DR scan
}

void send_read_buf_lsb1st(const uint8_t *buf, int bufLen, int last, uint8_t *w) {
    const uint8_t *p = buf;
    int l = bufLen;

    int val = 0;
    tms.off();

    for (int i = 0; i < l - 1; i++) {
        uint8_t byte = 0;
        uint8_t val = p[i];

        for (int nf = 0; nf < 8; nf++) {
            if ((val >> nf) & 1) {
                tdi.on();
            } else {
                tdi.off();
            }
            tck.off();
            tck.on();
            if (tdo.value()) {
                byte |= 1 << nf;
            }
        }

        if (w) {
            w[i] = byte; // # write byte
        }
    }

    uint8_t byte = 0;
    val = p[l - 1]; // # read last byte
    for (int nf = 0; nf < 7; nf++) { // # first 7 bits
        if ((val >> nf) & 1) {
            tdi.on();
        } else {
            tdi.off();
        }
        tck.off();
        tck.on();
        if (tdo.value()) {
            byte |= 1 << nf;
        }
    }

    // # last bit
    if (last) {
        tms.on();
    }
    if ((val >> 7) & 1) {
        tdi.on();
    } else {
        tdi.off();
    }
    tck.off();
    tck.on();
    if (tdo.value()) {
        byte |= 1 << 7;
    }
    if (w) {
        w[l - 1] = byte; //# write last byte
    }
}

void reset_tap() {
    for (int n = 0; n < 6; n++) {
        send_tms(1); // # -> Test Logic Reset
    }
}

void runtest_idle(int count, uint32_t duration_ms) {
    uint32_t leave = millis() + duration_ms;

    for (int n = 0; n < count; n++) {
        send_tms(0); // # -> idle
    }

    while (leave >= millis()) {
        send_tms(0); // # -> idle
    }

    send_tms(1); // # -> select DR scan
}

void sir(const uint8_t *buf, int bufLen) {
    send_tms(1); // # -> select IR scan
    send_tms(0); // # -> capture IR
    send_tms(0); // # -> shift IR
    send_read_buf_lsb1st(buf, bufLen, 1, 0); //# -> exit 1 IR
    send_tms0111(); // # -> select DR scan
}

void sir(uint8_t byte) {
    sir(&byte, 1);
}

void sir_idle(const uint8_t *buf, int bufLen, int n, uint32_t ms) {
    send_tms(1); // # -> select IR scan
    send_tms(0); // # -> capture IR
    send_tms(0); // # -> shift IR
    send_read_buf_lsb1st(buf, bufLen, 1, 0); // # -> exit 1 IR
    send_tms(0); // # -> pause IR
    send_tms(1); // # -> exit 2 IR
    send_tms(1); // # -> update IR
    runtest_idle(n + 1, ms); // # -> select DR scan
}

void sir_idle(uint8_t byte, int n, uint32_t ms) {
    sir_idle(&byte, 1, n, ms);
}

void sdr(const uint8_t *buf, int bufLen) {
    send_tms(0); // # ->capture DR
    send_tms(0); // # ->shift DR
    send_read_buf_lsb1st(buf, bufLen, 1, 0);
    send_tms0111(); // # -> select DR scan
}

void sdr(uint8_t byte) {
    sdr(&byte, 1);
}

void sdr_idle(const uint8_t *buf, int bufLen, int n, uint32_t ms) {
    send_tms(0); // # -> capture DR
    send_tms(0); // # -> shift DR
    send_read_buf_lsb1st(buf, bufLen, 1, 0);
    send_tms(0); // # -> pause DR
    send_tms(1); // # -> exit 2 DR
    send_tms(1); // # -> update DR
    runtest_idle(n + 1, ms); // # -> select DR scan
}

void sdr_idle(uint8_t byte, int n, uint32_t ms) {
    sdr_idle(&byte, 1, n, ms);
}

void sdr_response(uint8_t *buf, int bufLen) {
    send_tms(0); // # -> capture DR
    send_tms(0); // # -> shift DR
    send_read_buf_lsb1st(buf, bufLen, 1, buf);
    send_tms0111(); // # -> select DR scan
}

void check_response(uint32_t response, uint32_t expected, uint32_t mask = 0xFFFFFFFF, const char *message = "") {
    if ((response & mask) != expected) {
        DebugTrace("0x%08X & 0x%08X != 0x%08X %s\n", response, mask, expected, message);
    }
}

void common_open() {
    spi_jtag_on();

    // hwspi.init(sck=Pin(gpio_tcknc)) # avoid TCK-glitch

    bitbang_jtag_on();

    reset_tap();
    runtest_idle(1, 0);

    //#sir(b"\xE0") # read IDCODE
    //#sdr(pack("<I",0), expected=pack("<I",0), message="IDCODE")

    sir(0x1C); // # LSC_PRELOAD: program Bscan register

    uint8_t buf[64];
    for (int i = 0; i < 64; i++) {
        buf[i] = 0xFF;
    }
    sdr(buf, 64);

    sir(0xC6); // # ISC ENABLE: Enable SRAM programming mode

    sdr_idle(0x00, 2, 10);
    sir_idle(0x3C, 2, 1); // # LSC_READ_STATUS

    uint32_t status;
    sdr_response((uint8_t *)&status, 4);
    check_response(status, 0, 0x24040, "FAIL status");

    sir(0x0E); //# ISC_ERASE: Erase the SRAM

    sdr_idle(0x01, 2, 10);
    sir_idle(0x3C, 2, 1); //# LSC_READ_STATUS

    sdr_response((uint8_t *)&status, 4);
    check_response(status, 0, 0xB000, "FAIL status");
}

void prog_open() {
    common_open();

    sir(0x46); // # LSC_INIT_ADDRESS
    sdr_idle(0x01, 2, 10);
    sir(0x7A); // # LSC_BITSTREAM_BURST
    // # ---------- bitstream begin -----------
    // # manually walk the TAP
    // # we will be sending one long DR command
    send_tms(0); // # ->capture DR
    send_tms(0); // # ->shift DR
    // # switch from bitbanging to SPI mode

    // hwspi.init(sck = Pin(gpio_tck)) // # 1 TCK - glitch ? TDI = 0

    // # we are lucky that format of the bitstream tolerates
    // # any leading and trailing junk bits. If it weren't so,
    // # HW SPI JTAG acceleration wouldn't work.
    // # to upload the bitstream:
    // # FAST SPI mode
    // #hwspi.write(block)
    // # SLOW bitbanging mode
    // #for byte in block:
    // #  send_int_msb1st(byte,0)

}

void prog_stream_done() {
    // # switch from hardware SPI to bitbanging done after prog_stream()
    // hwspi.init(sck=Pin(gpio_tcknc)) # avoid TCK-glitch
    spi_jtag_off();
}

// # call this after uploading all of the bitstream blocks,
// # this will exit FPGA programming mode and start the bitstream
// # returns status True - OK False - Fail
bool prog_close() {
    bitbang_jtag_on();
    
    send_tms(1); // # ->exit 1 DR
    send_tms(0); // # ->pause DR
    send_tms(1); // # ->exit 2 DR
    send_tms(1); // # ->update DR
    // #send_tms(0) # ->idle, disabled here as runtest_idle does the same
    runtest_idle(100, 10);
    // # ---------- bitstream end -----------
    sir_idle(0xC0, 2, 1); //# read usercode
    
    uint32_t usercode;
    sdr_response((uint8_t *)&usercode, 4);
    check_response(usercode, 0, 0xFFFFFFFF, "FAIL usercode");
    
    sir_idle(0x26, 2, 200); // # ISC DISABLE
    sir_idle(0xFF, 2, 1); // # BYPASS
    
    sir(0x3C); // # LSC_READ_STATUS
    
    uint32_t status;
    sdr_response((uint8_t *)&status, 4);
    check_response(status, 0x100, 0x2100, "FAIL status");

    bool done = true;
    if ((status & 0x2100) != 0x100) {
        done = false;
    }

    reset_tap();
    
    bitbang_jtag_off();

    return done;
}

void write_block(uint8_t *block, uint32_t blockLen) {
#if defined(EEZ_PLATFORM_STM32)
    spi::select(0, spi::CHIP_FPGA);
    spi::transmit(0, block, blockLen);
    spi::deselect(0);
#endif
}

scpi_result_t prog(const char *filePath) {
#if OPTION_DISPLAY
    size_t total;
    size_t transfered;
#endif

	File file;

    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        goto Exit;
    }

#if OPTION_DISPLAY
    psu::gui::showProgressPageWithoutAbort("Programming FPGA...");
    psu::gui::updateProgressPage(0, 0);
    total = file.size();
    transfered = 0;
#endif

    prog_open();

    while (true) {
        uint8_t block[1024];
        size_t bytes = file.read(block, sizeof(block));
        if (bytes == 0) {
            break;
        }

        write_block(block, bytes);

#if OPTION_DISPLAY
        transfered += bytes;
        psu::gui::updateProgressPage(transfered, total);
#endif

        if (bytes < sizeof(block)) {
            break;
        }
    }

    prog_stream_done();

    prog_close();

    file.close();

#if OPTION_DISPLAY
    psu::gui::hideProgressPage();
#endif

Exit:

    return SCPI_RES_OK;
}

} // namespace fpga
} // namespace eez
