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

#pragma once

#include <scpi/scpi.h>

namespace eez {
namespace scpi {

//
// QUEStionable status register bits
//
#define QUES_TIME (1 << 3)   /* TIME */
#define QUES_TEMP (1 << 4)   /* TEMPerature */
#define QUES_MMEM (1 << 5)   /* MMEM – SD card is not inserted */
#define QUES_FAN  (1 << 12)  /* FAN */
#define QUES_ISUM (1 << 13)  /* INSTrument Summary */

//
// OPERation Status Register bits
//
#define OPER_GROUP_COMMON_GND    (1 << 6) /* CGND */
#define OPER_GROUP_SPLIT_RAILS   (1 << 7) /* SRAil */
#define OPER_GROUP_PARALLEL      (1 << 8) /* GROUp PARAllel */
#define OPER_GROUP_SERIAL        (1 << 9) /* GROUp SERIal */

#define OPER_DLOG (1 << 10)          /* DLog is active */
#define OPER_ISUM (1 << 13)          /* INSTrument Summary */

//
// QUEStionable INSTrument register bits
//
#define QUES_ISUM1 (1 << 1) /* INSTrument 1 QUEStionable Event Summary */
#define QUES_ISUM2 (1 << 2) /* INSTrument 2 QUEStionable Event Summary */

//
// OPERation INSTrument register bits
//
#define OPER_ISUM1 (1 << 1) /* INSTrument 1 OPERation Event Summary */
#define OPER_ISUM2 (1 << 2) /* INSTrument 2 OPERation Event Summary */

//
// QUEStionable INSTrument ISUMmary register bits
//
#define QUES_ISUM_VOLT (1 << 0) /* VOLTage */
#define QUES_ISUM_CURR (1 << 1) /* CURRent */
#define QUES_ISUM_TEMP (1 << 4) /* TEMPerature */
#define QUES_ISUM_RPOL (1 << 7) /* Remote sense reverse polarity is detected */
#define QUES_ISUM_OVP (1 << 8)  /* OVP */
#define QUES_ISUM_OCP (1 << 9)  /* OVP */
#define QUES_ISUM_OPP (1 << 10) /* OPP */

//
// OPERation INSTrument ISUMmary register bits
//
#define OPER_ISUM_CALI (1 << 0)      /* CALIbrating */
#define OPER_ISUM_MEAS (1 << 4)      /* MEASuring */
#define OPER_ISUM_TRIG (1 << 5)      /* Waiting for TRIGger */
#define OPER_ISUM_CV (1 << 8)        /* Constant Voltage */
#define OPER_ISUM_CC (1 << 9)        /* Constant Current */
#define OPER_ISUM_OE_OFF (1 << 10)   /* Output Enable OFF */
#define OPER_ISUM_DP_OFF (1 << 11)   /* Down Programmer OFF */
#define OPER_ISUM_RSENS_ON (1 << 12) /* Remote SENSe ON */
#define OPER_ISUM_RPROG_ON (1 << 13) /* Remote PROGram ON */

//
// PSU registers
//
enum scpi_psu_reg_name_t {
    SCPI_PSU_REG_QUES_COND,
    SCPI_PSU_REG_OPER_COND,

    SCPI_PSU_REG_QUES_INST_COND,
    SCPI_PSU_REG_QUES_INST_EVENT,
    SCPI_PSU_REG_QUES_INST_ENABLE,

    SCPI_PSU_REG_OPER_INST_COND,
    SCPI_PSU_REG_OPER_INST_EVENT,
    SCPI_PSU_REG_OPER_INST_ENABLE,

    SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1,
    SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1,
    SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1,

    SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1,
    SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1,
    SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1,

    SCPI_PSU_CH_REG_QUES_INST_ISUM_COND2,
    SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT2,
    SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE2,

    SCPI_PSU_CH_REG_OPER_INST_ISUM_COND2,
    SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT2,
    SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE2,

    SCPI_PSU_REG_COUNT,
};

void scpi_reg_set(scpi_reg_name_t name, scpi_reg_val_t val);

scpi_reg_val_t reg_get(scpi_t *context, scpi_psu_reg_name_t name);
void reg_set(scpi_t *context, scpi_psu_reg_name_t name, scpi_reg_val_t val);

void reg_set(scpi_psu_reg_name_t name, scpi_reg_val_t val);

void reg_set_esr_bits(int bit_mask);

void reg_set_ques_bit(int bit_mask, bool on);
void reg_set_ques_isum_bit(int iChannel /* zero based */, int bit_mask, bool on);

void reg_set_oper_bit(int bit_mask, bool on);

void reg_set_oper_isum_bit(int iChannel /* zero based */, int bit_mask, bool on);

} // namespace scpi
} // namespace eez
