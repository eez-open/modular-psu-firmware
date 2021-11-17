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

#include <bb3/psu/psu.h>

#include <bb3/psu/scpi/psu.h>

namespace eez {

using namespace scpi;

namespace psu {
namespace scpi {

////////////////////////////////////////////////////////////////////////////////

scpi_result_t scpi_cmd_statusQuestionableEventQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_QUES));

    /* clear register */
    SCPI_RegSet(context, SCPI_REG_QUES, 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusQuestionableConditionQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, reg_get(context, SCPI_PSU_REG_QUES_COND));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusQuestionableEnable(scpi_t *context) {
    int32_t new_QUESE;
    if (SCPI_ParamInt32(context, &new_QUESE, TRUE)) {
        SCPI_RegSet(context, SCPI_REG_QUESE, (scpi_reg_val_t)new_QUESE);
        return SCPI_RES_OK;
    }
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_statusQuestionableEnableQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_QUESE));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationEventQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_OPER));

    /* clear register */
    SCPI_RegSet(context, SCPI_REG_OPER, 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationConditionQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, reg_get(context, SCPI_PSU_REG_OPER_COND));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationEnable(scpi_t *context) {
    int32_t new_OPERE;
    if (SCPI_ParamInt32(context, &new_OPERE, TRUE)) {
        SCPI_RegSet(context, SCPI_REG_OPERE, (scpi_reg_val_t)new_OPERE);
        return SCPI_RES_OK;
    }
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_statusOperationEnableQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, SCPI_RegGet(context, SCPI_REG_OPERE));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusQuestionableInstrumentEventQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, reg_get(context, SCPI_PSU_REG_QUES_INST_EVENT));

    /* clear register */
    reg_set(context, SCPI_PSU_REG_QUES_INST_EVENT, 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusQuestionableInstrumentConditionQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, reg_get(context, SCPI_PSU_REG_QUES_INST_COND));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusQuestionableInstrumentEnable(scpi_t *context) {
    int32_t newVal;
    if (SCPI_ParamInt32(context, &newVal, TRUE)) {
        reg_set(context, SCPI_PSU_REG_QUES_INST_ENABLE, (scpi_reg_val_t)newVal);
        return SCPI_RES_OK;
    }
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_statusQuestionableInstrumentEnableQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, reg_get(context, SCPI_PSU_REG_QUES_INST_ENABLE));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationInstrumentEventQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, reg_get(context, SCPI_PSU_REG_OPER_INST_EVENT));

    /* clear register */
    reg_set(context, SCPI_PSU_REG_OPER_INST_EVENT, 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationInstrumentConditionQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, reg_get(context, SCPI_PSU_REG_OPER_INST_COND));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationInstrumentEnable(scpi_t *context) {
    int32_t newVal;
    if (SCPI_ParamInt32(context, &newVal, TRUE)) {
        reg_set(context, SCPI_PSU_REG_OPER_INST_ENABLE, (scpi_reg_val_t)newVal);
        return SCPI_RES_OK;
    }
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_statusOperationInstrumentEnableQ(scpi_t *context) {
    /* return value */
    SCPI_ResultInt32(context, reg_get(context, SCPI_PSU_REG_OPER_INST_ENABLE));

    return SCPI_RES_OK;
}

static int getSelectedChannelIndexFromCommandNumberOrContext(scpi_t *context) {
    int32_t channelIndex;
    SCPI_CommandNumbers(context, &channelIndex, 1, -1);
    if (channelIndex != -1) {
        channelIndex--;
        if (channelIndex < 0 || channelIndex > MIN(CH_NUM, 2) - 1) {
            SCPI_ErrorPush(context, SCPI_ERROR_HEADER_SUFFIX_OUTOFRANGE);
            return -1;
        }
        return channelIndex;
    } 

    auto channel = getSelectedPowerChannel(context);
    if (!channel) {
        return -1;
    }

    return channel->channelIndex;
}

scpi_result_t scpi_cmd_statusQuestionableInstrumentIsummaryEventQ(scpi_t *context) {
    int32_t channelIndex = getSelectedChannelIndexFromCommandNumberOrContext(context);
    if (channelIndex == -1 || channelIndex >= NUM_REG_INSTRUMENTS) {
        return SCPI_RES_ERR;
    }

    scpi_psu_reg_name_t isumReg = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_EVENT1 + channelIndex);

    /* return value */
    SCPI_ResultInt32(context, reg_get(context, isumReg));

    /* clear register */
    reg_set(context, isumReg, 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusQuestionableInstrumentIsummaryConditionQ(scpi_t *context) {
    int32_t channelIndex = getSelectedChannelIndexFromCommandNumberOrContext(context);
    if (channelIndex == -1 || channelIndex >= NUM_REG_INSTRUMENTS) {
        return SCPI_RES_ERR;
    }

    scpi_psu_reg_name_t isumReg = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_COND1 + channelIndex);

    /* return value */
    SCPI_ResultInt32(context, reg_get(context, isumReg));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusQuestionableInstrumentIsummaryEnable(scpi_t *context) {
    int32_t channelIndex = getSelectedChannelIndexFromCommandNumberOrContext(context);
    if (channelIndex == -1 || channelIndex >= NUM_REG_INSTRUMENTS) {
        return SCPI_RES_ERR;
    }

    scpi_psu_reg_name_t isumeReg = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1 + channelIndex);

    int32_t newVal;
    if (SCPI_ParamInt32(context, &newVal, TRUE)) {
        reg_set(context, isumeReg, (scpi_reg_val_t)newVal);
        return SCPI_RES_OK;
    }
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_statusQuestionableInstrumentIsummaryEnableQ(scpi_t *context) {
    int32_t channelIndex = getSelectedChannelIndexFromCommandNumberOrContext(context);
    if (channelIndex == -1 || channelIndex >= NUM_REG_INSTRUMENTS) {
        return SCPI_RES_ERR;
    }

    scpi_psu_reg_name_t isumeReg = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1 + channelIndex);

    /* return value */
    SCPI_ResultInt32(context, reg_get(context, isumeReg));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationInstrumentIsummaryEventQ(scpi_t *context) {
    int32_t channelIndex = getSelectedChannelIndexFromCommandNumberOrContext(context);
    if (channelIndex == -1 || channelIndex >= NUM_REG_INSTRUMENTS) {
        return SCPI_RES_ERR;
    }

    scpi_psu_reg_name_t isumReg = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_EVENT1 + channelIndex);

    /* return value */
    SCPI_ResultInt32(context, reg_get(context, isumReg));

    /* clear register */
    reg_set(context, isumReg, 0);

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationInstrumentIsummaryConditionQ(scpi_t *context) {
    int32_t channelIndex = getSelectedChannelIndexFromCommandNumberOrContext(context);
    if (channelIndex == -1 || channelIndex >= NUM_REG_INSTRUMENTS) {
        return SCPI_RES_ERR;
    }

    scpi_psu_reg_name_t isumReg = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_COND1 + channelIndex);

    /* return value */
    SCPI_ResultInt32(context, reg_get(context, isumReg));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusOperationInstrumentIsummaryEnable(scpi_t *context) {
    int32_t channelIndex = getSelectedChannelIndexFromCommandNumberOrContext(context);
    if (channelIndex == -1 || channelIndex >= NUM_REG_INSTRUMENTS) {
        return SCPI_RES_ERR;
    }

    scpi_psu_reg_name_t isumeReg = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1 + channelIndex);

    int32_t newVal;
    if (SCPI_ParamInt32(context, &newVal, TRUE)) {
        reg_set(context, isumeReg, (scpi_reg_val_t)newVal);
        return SCPI_RES_OK;
    }
    return SCPI_RES_ERR;
}

scpi_result_t scpi_cmd_statusOperationInstrumentIsummaryEnableQ(scpi_t *context) {
    int32_t channelIndex = getSelectedChannelIndexFromCommandNumberOrContext(context);
    if (channelIndex == -1 || channelIndex >= NUM_REG_INSTRUMENTS) {
        return SCPI_RES_ERR;
    }

    scpi_psu_reg_name_t isumeReg = (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1 + channelIndex);

    /* return value */
    SCPI_ResultInt32(context, reg_get(context, isumeReg));

    return SCPI_RES_OK;
}

scpi_result_t scpi_cmd_statusPreset(scpi_t *context) {
    SCPI_RegSet(context, SCPI_REG_ESE, 0);

    SCPI_RegSet(context, SCPI_REG_QUESE, 0);
    SCPI_RegSet(context, SCPI_REG_OPERE, 0);

    reg_set(context, SCPI_PSU_REG_QUES_INST_ENABLE, 0);
    reg_set(context, SCPI_PSU_REG_OPER_INST_ENABLE, 0);

    for (int i = 0; i < NUM_REG_INSTRUMENTS; i++) {
        reg_set(context, (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_QUES_INST_ISUM_ENABLE1 + i), 0);
        reg_set(context, (scpi_psu_reg_name_t)(SCPI_PSU_CH_REG_OPER_INST_ISUM_ENABLE1 + i), 0);
    }

    return SCPI_RES_OK;
}

} // namespace scpi
} // namespace psu
} // namespace eez
