/*
 * EEZ PSU Firmware
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

#define CH_BOARD_REVISION_NONE        0
#define CH_BOARD_REVISION_DCP505_R1B3 1
#define CH_BOARD_REVISION_DCP405_R1B1 2
#define CH_BOARD_REVISION_DCP405_R2B5 3
#define CH_BOARD_REVISION_DCM220_R1B1 4

//                       OVP_DEFAULT_STATE, OVP_MIN_DELAY, OVP_DEFAULT_DELAY, OVP_MAX_DELAY
#define CH_PARAMS_OVP false, 0.0f, 0.005f, 10.0f

//                      U_MIN, U_DEF, U_MAX, U_MAX_CONF, U_MIN_STEP, U_DEF_STEP, U_MAX_STEP, U_CAL_VAL_MIN, U_CAL_VAL_MID, U_CAL_VAL_MAX, U_CURR_CAL
#define CH_PARAMS_U_50V 0.0f,  0.0f,  50.0f, 50.0f,      0.01f,      0.1f,       5.0f,       0.15f,         24.1f,         48.0f,         25.0f,     CH_PARAMS_OVP

//                      U_MIN, U_DEF, U_MAX, U_MAX_CONF, U_MIN_STEP, U_DEF_STEP, U_MAX_STEP, U_CAL_VAL_MIN, U_CAL_VAL_MID, U_CAL_VAL_MAX, U_CURR_CAL
#define CH_PARAMS_U_40V 0.0f,  0.0f,  40.0f, 40.0f,      0.01f,      0.1f,       5.0f,       0.15f,         20.0f,         38.0f,         20.0f,     CH_PARAMS_OVP

//                      U_MIN, U_DEF, U_MAX, U_MAX_CONF, U_MIN_STEP, U_DEF_STEP, U_MAX_STEP, U_CAL_VAL_MIN, U_CAL_VAL_MID, U_CAL_VAL_MAX, U_CURR_CAL
#define CH_PARAMS_U_20V 0.0f,  0.0f,  20.0f, 20.0f,      0.01f,      0.1f,       5.0f,       0.15f,         10.0f,         18.0f,         10.0f,     CH_PARAMS_OVP

//                    OCP_DEFAULT_STATE, OCP_MIN_DELAY, OCP_DEFAULT_DELAY, OCP_MAX_DELAY
#define CH_PARAMS_OCP false,             0.0f,          0.02f,             10.0f

//                     I_MIN, I_DEF, I_MAX,  I_MAX_CONF, I_MIN_STEP, I_DEF_STEP, I_MAX_STEP, I_CAL_VAL_MIN, I_CAL_VAL_MID, I_CAL_VAL_MAX, I_VOLT_CAL
#define CH_PARAMS_I_5A 0.0f,  0.0f,  5.0f,   5.0f,       0.01f,      0.01f,      1.0f,       0.05f,         2.425f,        4.8f,          0.1f,      CH_PARAMS_OCP

//                     I_MIN, I_DEF, I_MAX,  I_MAX_CONF, I_MIN_STEP, I_DEF_STEP, I_MAX_STEP, I_CAL_VAL_MIN, I_CAL_VAL_MID, I_CAL_VAL_MAX, I_VOLT_CAL
#define CH_PARAMS_I_4A 0.0f,  0.0f,  4.0f,   4.0f,       0.01f,      0.01f,      1.0f,       0.05f,         1.95f,         3.8f,          0.1f,      CH_PARAMS_OCP

//                          OPP_MIN_DELAY, OPP_DEFAULT_DELAY, OPP_MAX_DELAY
#define CH_PARAMS_OPP_DELAY 1.0f,          10.0f,             300.0f

//                                                        OPP_DEFAULT_STATE,         OPP_MIN_LEVEL, SOA_VIN, OPP_DEFAULT_LEVEL, SOA_PREG_CURR, OPP_MAX_LEVEL, SOA_POSTREG_PTOT, PTOT
#define CH_PARAMS_NONE   CH_PARAMS_U_50V, CH_PARAMS_I_5A, true, CH_PARAMS_OPP_DELAY, 0.0f,          160.0f,  200.0f,            58.0f,         5.0f,          25.0f,            200.0f

//                                                        OPP_DEFAULT_STATE,         OPP_MIN_LEVEL, SOA_VIN, OPP_DEFAULT_LEVEL, SOA_PREG_CURR, OPP_MAX_LEVEL, SOA_POSTREG_PTOT, PTOT
#define CH_PARAMS_50V_5A CH_PARAMS_U_50V, CH_PARAMS_I_5A, true, CH_PARAMS_OPP_DELAY, 0.0f,          160.0f,  200.0f,            58.0f,         5.0f,          25.0f,            200.0f

//                                                        OPP_DEFAULT_STATE,         OPP_MIN_LEVEL, SOA_VIN, OPP_DEFAULT_LEVEL, SOA_PREG_CURR, OPP_MAX_LEVEL, SOA_POSTREG_PTOT, PTOT
#define CH_PARAMS_40V_5A CH_PARAMS_U_40V, CH_PARAMS_I_5A, true, CH_PARAMS_OPP_DELAY, 0.0f,          160.0f,  200.0f,            58.0f,         5.0f,          25.0f,            200.0f

//                                                        OPP_DEFAULT_STATE,         OPP_MIN_LEVEL, SOA_VIN, OPP_DEFAULT_LEVEL, SOA_PREG_CURR, OPP_MAX_LEVEL, SOA_POSTREG_PTOT, PTOT
#define CH_PARAMS_20V_4A CH_PARAMS_U_20V, CH_PARAMS_I_4A, true, CH_PARAMS_OPP_DELAY, 0.0f,           70.0f,   80.0f,            58.0f,         5.0f,          25.0f,             80.0f
