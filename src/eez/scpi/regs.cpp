/*
 * EEZ Modular Firmware
 * Copyright (C) 2015-present, Envox d.o.o.
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

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/scpi/psu.h>
#include <eez/modules/psu/serial_psu.h>
#if OPTION_ETHERNET
#include <eez/modules/psu/ethernet.h>
#endif

using namespace eez::psu;
using namespace eez::psu::scpi;

namespace eez {
namespace scpi {

void scpi_reg_set(scpi_reg_name_t name, scpi_reg_val_t val) {
    if (serial::g_testResult == TEST_OK) {
        SCPI_RegSet(&serial::g_scpiContext, name, val);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        SCPI_RegSet(&ethernet::g_scpiContext, name, val);
    }
#endif
}


/**
 * Update register value
 * @param context
 * @param name - register name
 */
static void psu_reg_update(scpi_t *context, scpi_psu_reg_name_t name) {
    reg_set(context, name, reg_get(context, name));
}

/**
 * Update IEEE 488.2 register according to value and its mask register
 * @param context
 * @param val value of register
 * @param mask name of mask register (enable register)
 * @param bits bits to clear or set in STB
 */
static void psu_reg_update_ieee488_reg(scpi_t *context, scpi_reg_val_t val,
                                       scpi_psu_reg_name_t mask, scpi_reg_name_t ieee488RegName,
                                       scpi_reg_val_t bits) {
    if (val & reg_get(context, mask)) {
        SCPI_RegSetBits(context, ieee488RegName, bits);
    } else {
        SCPI_RegClearBits(context, ieee488RegName, bits);
    }
}

/**
 * Set PSU register bits
 * @param name - register name
 * @param bits bit mask
 */
void psu_reg_set_bits(scpi_t *context, scpi_psu_reg_name_t name, scpi_reg_val_t bits) {
    reg_set(context, name, reg_get(context, name) | bits);
}

/**
 * Clear PSU register bits
 * @param name - register name
 * @param bits bit mask
 */
void psu_reg_clear_bits(scpi_t *context, scpi_psu_reg_name_t name, scpi_reg_val_t bits) {
    reg_set(context, name, reg_get(context, name) & ~bits);
}

/**
 * Update PSU register according to value and its mask register
 * @param context
 * @param val value of register
 * @param mask name of mask register (enable register)
 * @param bits bits to clear or set in STB
 */
static void psu_reg_update_psu_reg(scpi_t *context, scpi_reg_val_t val, scpi_psu_reg_name_t mask,
                                   scpi_psu_reg_name_t psuRegName, scpi_reg_val_t bits) {
    if (val & reg_get(context, mask)) {
        psu_reg_set_bits(context, psuRegName, bits);
    } else {
        psu_reg_clear_bits(context, psuRegName, bits);
    }
    psu_reg_update(context, psuRegName);
}

/**
 * Update PSU register according to value
 * @param context
 * @param val value of register
 * @param mask name of mask register (enable register)
 * @param bits bits to clear or set in STB
 */
static void psu_reg_update_psu_reg(scpi_t *context, scpi_reg_val_t val,
                                   scpi_psu_reg_name_t psuRegName, scpi_reg_val_t bits) {
    if (val) {
        psu_reg_set_bits(context, psuRegName, bits);
    } else {
        psu_reg_clear_bits(context, psuRegName, bits);
    }
    psu_reg_update(context, psuRegName);
}

/**
 * Get PSU specific register value
 * @param name - register name
 * @return register value
 */
scpi_reg_val_t reg_get(scpi_t *context, scpi_psu_reg_name_t name) {
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    if ((name < SCPI_PSU_REG_COUNT) && (psu_context->registers != NULL)) {
        return psu_context->registers[name];
    } else {
        return 0;
    }
}

/**
 * Set PSU specific register value
 * @param name - register name
 * @param val - new value
 */
void reg_set(scpi_t *context, scpi_psu_reg_name_t name, scpi_reg_val_t val) {
    scpi_psu_t *psu_context = (scpi_psu_t *)context->user_context;
    if ((name >= SCPI_PSU_REG_COUNT) || (psu_context->registers == NULL)) {
        return;
    }

    /* set register value */
    psu_context->registers[name] = val;

    switch (name) {
    case SCPI_PSU_REG_QUES_INST_COND:
        psu_reg_update_psu_reg(context, val, SCPI_PSU_REG_QUES_COND, QUES_ISUM);
        break;
    case SCPI_PSU_REG_QUES_INST_EVENT:
        psu_reg_update_ieee488_reg(context, val, SCPI_PSU_REG_QUES_INST_ENABLE, SCPI_REG_QUES,
                                   QUES_ISUM);
        break;
    case SCPI_PSU_REG_QUES_INST_ENABLE:
        psu_reg_update(context, SCPI_PSU_REG_QUES_INST_EVENT);
        break;

    case SCPI_PSU_REG_OPER_INST_COND:
        psu_reg_update_psu_reg(context, val, SCPI_PSU_REG_OPER_COND, OPER_ISUM);
        break;
    case SCPI_PSU_REG_OPER_INST_EVENT:
        psu_reg_update_ieee488_reg(context, val, SCPI_PSU_REG_OPER_INST_ENABLE, SCPI_REG_OPER,
                                   OPER_ISUM);
        break;
    case SCPI_PSU_REG_OPER_INST_ENABLE:
        psu_reg_update(context, SCPI_PSU_REG_OPER_INST_EVENT);
        break;

    default:
    	break;
    }

    if (name >= SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1 && name < SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1 + NUM_REG_INSTRUMENTS) {
        int instrumentIndex = name - SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1;
        psu_reg_update_psu_reg(context, val, SCPI_PSU_REG_QUES_INST_COND, QUES_ISUM_N(instrumentIndex));
    } else if (name >= SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1 && name < SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1 + NUM_REG_INSTRUMENTS) {
        int instrumentIndex = name - SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1;
        psu_reg_update_psu_reg(context, val, (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1 + instrumentIndex), SCPI_PSU_REG_QUES_INST_EVENT, QUES_ISUM_N(instrumentIndex));
    } else if (name >= SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1 && name < SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1 + NUM_REG_INSTRUMENTS) {
        int instrumentIndex = name - SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1;
        psu_reg_update(context, (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1 + instrumentIndex));
    } else if (name >= SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1 && name < SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1 + NUM_REG_INSTRUMENTS) {
        int instrumentIndex = name - SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1;
        psu_reg_update_psu_reg(context, val, SCPI_PSU_REG_OPER_INST_COND, OPER_ISUM_N(instrumentIndex));
    } else if (name >= SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1 && name < SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1 + NUM_REG_INSTRUMENTS) {
        int instrumentIndex = name - SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1;
        psu_reg_update_psu_reg(context, val, (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1 + instrumentIndex), SCPI_PSU_REG_OPER_INST_EVENT, OPER_ISUM_N(instrumentIndex));
    } else if (name >= SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1 && name < SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1 + NUM_REG_INSTRUMENTS) {
        int instrumentIndex = name - SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1;
        psu_reg_update(context, (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1 + instrumentIndex));
    }
}

void reg_set(scpi_psu_reg_name_t name, scpi_reg_val_t val) {
    if (serial::g_testResult == TEST_OK) {
        reg_set(&serial::g_scpiContext, name, val);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        reg_set(&ethernet::g_scpiContext, name, val);
    }
#endif
}

void reg_set_esr_bits(int bit_mask) {
    if (serial::g_testResult == TEST_OK) {
        SCPI_RegSetBits(&serial::g_scpiContext, SCPI_REG_ESR, bit_mask);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        SCPI_RegSetBits(&ethernet::g_scpiContext, SCPI_REG_ESR, bit_mask);
    }
#endif
}

void reg_set_ques_bit(scpi_t *context, int bit_mask, bool on) {
    scpi_reg_val_t val = reg_get(context, SCPI_PSU_REG_QUES_COND);
    if (on) {
        if (!(val & bit_mask)) {
            reg_set(context, SCPI_PSU_REG_QUES_COND, val | bit_mask);

            // set event on raising condition
            val = SCPI_RegGet(context, SCPI_REG_QUES);
            SCPI_RegSet(context, SCPI_REG_QUES, val | bit_mask);
        }
    } else {
        if (val & bit_mask) {
            reg_set(context, SCPI_PSU_REG_QUES_COND, val & ~bit_mask);
        }
    }
}

void reg_set_ques_bit(int bit_mask, bool on) {
    if (serial::g_testResult == TEST_OK) {
        reg_set_ques_bit(&serial::g_scpiContext, bit_mask, on);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        reg_set_ques_bit(&ethernet::g_scpiContext, bit_mask, on);
    }
#endif
}

void reg_set_ques_isum_bit(scpi_t *context, int iChannel, int bit_mask, bool on) {
    if (iChannel >= NUM_REG_INSTRUMENTS) {
        return;
    }

    scpi_psu_reg_name_t reg_name = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1 + iChannel);
    scpi_reg_val_t val = reg_get(context, reg_name);
    if (on) {
        if (!(val & bit_mask)) {
            reg_set(context, reg_name, val | bit_mask);

            // set event on raising condition
            reg_name = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1 + iChannel);
            val = reg_get(context, reg_name);
            reg_set(context, reg_name, val | bit_mask);
        }
    } else {
        if (val & bit_mask) {
            reg_set(context, reg_name, val & ~bit_mask);
        }
    }
}

void reg_set_ques_isum_bit(int iChannel, int bit_mask, bool on) {
    if (serial::g_testResult == TEST_OK) {
        reg_set_ques_isum_bit(&serial::g_scpiContext, iChannel, bit_mask, on);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        reg_set_ques_isum_bit(&ethernet::g_scpiContext, iChannel, bit_mask, on);
    }
#endif
}

bool is_ques_bit_enabled(int channelIndex, int bit_mask) {
    if (serial::g_testResult == TEST_OK) {
        scpi_reg_val_t val = reg_get(&serial::g_scpiContext, (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1 + channelIndex));
        if (!(val & bit_mask)) {
            return false;
        }
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        scpi_reg_val_t val = reg_get(&ethernet::g_scpiContext, (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1 + channelIndex));
        if (!(val & bit_mask)) {
            return false;
        }
    }
#endif

    return true;
}

void reg_set_oper_bit(scpi_t *context, int bit_mask, bool on) {
    scpi_reg_val_t val = reg_get(context, SCPI_PSU_REG_OPER_COND);
    if (on) {
        if (!(val & bit_mask)) {
            reg_set(context, SCPI_PSU_REG_OPER_COND, val | bit_mask);

            // set event on raising condition
            val = SCPI_RegGet(context, SCPI_REG_OPER);
            SCPI_RegSet(context, SCPI_REG_OPER, val | bit_mask);
        }
    } else {
        if (val & bit_mask) {
            reg_set(context, SCPI_PSU_REG_OPER_COND, val & ~bit_mask);
        }
    }
}

void reg_set_oper_bit(int bit_mask, bool on) {
    if (serial::g_testResult == TEST_OK) {
        reg_set_oper_bit(&serial::g_scpiContext, bit_mask, on);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        reg_set_oper_bit(&ethernet::g_scpiContext, bit_mask, on);
    }
#endif
}


void reg_set_oper_isum_bit(scpi_t *context, int iChannel, int bit_mask, bool on) {
    if (iChannel >= NUM_REG_INSTRUMENTS) {
        return;
    }

    scpi_psu_reg_name_t reg_name = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1 + iChannel);
    scpi_reg_val_t val = reg_get(context, reg_name);
    if (on) {
        if (!(val & bit_mask)) {
            reg_set(context, reg_name, val | bit_mask);

            // set event on raising condition
            reg_name = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1 + iChannel);
            val = reg_get(context, reg_name);
            reg_set(context, reg_name, val | bit_mask);
        }
    } else {
        if (val & bit_mask) {
            reg_set(context, reg_name, val & ~bit_mask);
        }
    }
}

void reg_set_oper_isum_bit(int iChannel, int bit_mask, bool on) {
    if (serial::g_testResult == TEST_OK) {
        reg_set_oper_isum_bit(&serial::g_scpiContext, iChannel, bit_mask, on);
    }
#if OPTION_ETHERNET
    if (ethernet::g_testResult == TEST_OK) {
        reg_set_oper_isum_bit(&ethernet::g_scpiContext, iChannel, bit_mask, on);
    }
#endif
}

} // namespace scpi
} // namespace eez
