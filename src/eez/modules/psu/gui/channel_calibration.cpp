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

#if OPTION_DISPLAY

#include <string.h>
#include <stdio.h>

#include <eez/system.h>
#include <eez/hmi.h>

#include <eez/modules/psu/psu.h>
#include <eez/modules/psu/channel_dispatcher.h>
#include <eez/modules/psu/datetime.h>
#include <eez/modules/psu/list_program.h>
#include <eez/modules/psu/profile.h>
#include <eez/modules/psu/temperature.h>
#include <eez/modules/psu/trigger.h>

#include <eez/modules/psu/gui/psu.h>
#include <eez/modules/psu/gui/file_manager.h>
#include <eez/modules/psu/gui/keypad.h>
#include <eez/modules/psu/gui/edit_mode.h>
#include <eez/modules/psu/gui/channel_calibration.h>
#include <eez/modules/psu/gui/password.h>

#include <scpi/scpi.h>

namespace eez {
namespace psu {
namespace gui {

enum Align {
    ALIGN_LEFT = 0,
    ALIGN_TOP = 0,
    ALIGN_CENTER = 1,
    ALIGN_RIGHT = 2,
    ALIGN_BOTTOM = 2,
};

void drawGlyph(font::Font &font, font::Glyph &glyph, uint8_t encoding, int x, int y, Align horz, Align vert, int clip_x1, int clip_y1, int clip_x2, int clip_y2) {
    x -= glyph.x;
    y -= font.getAscent() - (glyph.y + glyph.height);

    if (horz == 1) {
        x -= glyph.width / 2;
    } else if (horz == 2) {
        x -= glyph.width;
    }

    if (vert == 1) {
        y -= glyph.height / 2;
    } else if (vert == 2) {
        y -= glyph.height;
    }

    char str[2] = { (char)encoding, 0 };

    mcu::display::drawStr(str, 1, x, y, clip_x1, clip_y1, clip_x2, clip_y2, font, -1);
}

void drawCalibrationChart(calibration::CalibrationBase &calibrationBase, const WidgetCursor &widgetCursor) {
    CalibrationValueType calibrationValueType;
    float dacValue;
    int chartZoom;

    auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    ChSettingsCalibrationViewPage *viewPage = nullptr;
    if (editPage) {
        calibrationValueType = editPage->getCalibrationValueType();
        dacValue = editPage->getDacValue();
        chartZoom = editPage->getChartZoom();
    } else {
        viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        calibrationValueType = viewPage->getCalibrationValueType();
        dacValue = viewPage->getDacValue();
        chartZoom = viewPage->getChartZoom();
    }

    auto &configuration = editPage ? editPage->getCalibrationValue().configuration : viewPage->getCalibrationValueConfiguration();

    const Widget *widget = widgetCursor.widget;
    const Style* style = getStyle(widget->style);

    int x = widgetCursor.x;
    int y = widgetCursor.y;
    int w = (int)widget->w;
    int h = (int)widget->h;

    drawRectangle(x, y, w, h, style, false, false, true);

    font::Font font(getFontData(FONT_ID_ROBOTO_CONDENSED_REGULAR));
    font::Glyph glyphLabel;
    font.getGlyph('0', glyphLabel);

    static const int GAP_BETWEEN_LABEL_AND_AXIS = 4;
    static const int GAP_BETWEEN_LABEL_AND_EDGE = 4;
    static const int MARGIN = GAP_BETWEEN_LABEL_AND_AXIS + glyphLabel.height + GAP_BETWEEN_LABEL_AND_EDGE;

    float minLimit;
    Unit unit;
    calibrationBase.getMinValue(calibrationValueType, minLimit, unit);

    float maxLimit;
    calibrationBase.getMaxValue(calibrationValueType, maxLimit, unit);

    float d = (maxLimit - minLimit) / chartZoom;

    float min = dacValue - d / 2;
    float max;
    if (min < minLimit) {
        min = minLimit;
        max = minLimit + d;
    } else {
        max = dacValue + d / 2;
        if (max > maxLimit) {
            max = maxLimit;
        }
        min = max - d;
   }

    auto scale = [&](float value) {
        return (value - min) / d;
    };

    // draw x axis labels
    mcu::display::setColor(style->color);

    char text[128];
    int labelTextWidth;
    int xLabelText;

    // min
    Value minValue = Value(min, unit);
    minValue.toText(text, sizeof(text));
    labelTextWidth = mcu::display::measureStr(text, -1, font);
    xLabelText = x + MARGIN - labelTextWidth / 2;
    if (xLabelText < x) {
        xLabelText = x;
    }
    mcu::display::drawStr(text, -1,
        xLabelText,
        y + h - MARGIN + GAP_BETWEEN_LABEL_AND_AXIS - (font.getAscent() - glyphLabel.height),
        x, y, x + w - 1, y + w - 1,
        font, -1);

    // max
    Value maxValue = Value(max, unit);
    maxValue.toText(text, sizeof(text));
    labelTextWidth = mcu::display::measureStr(text, -1, font);
    xLabelText = x + w - MARGIN - labelTextWidth / 2;
    if (xLabelText + labelTextWidth > x + w) {
        xLabelText = x + w - labelTextWidth;
    }
    mcu::display::drawStr(text, -1,
        xLabelText,
        y + h - MARGIN + GAP_BETWEEN_LABEL_AND_AXIS - (font.getAscent() - glyphLabel.height),
        x, y, x + w - 1, y + w - 1,
        font, -1);

    // draw diagonal line from (min, min) to (max, max)
    drawAntialiasedLine(x + MARGIN, y + h - MARGIN - 1, x + w - MARGIN - 1, y + MARGIN);

    // draw points
    mcu::display::setColor(COLOR_ID_YT_GRAPH_Y1);

    float xPoints[MAX_CALIBRATION_POINTS + 2];
    float yPoints[MAX_CALIBRATION_POINTS + 2];

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        xPoints[i + 1] = (w - 2 * MARGIN - 1) * scale(configuration.points[i].dac);
        yPoints[i + 1] = (h - 2 * MARGIN - 1) * scale(configuration.points[i].value);
    }

    if (configuration.numPoints > 1) {
        xPoints[0] = (w - 2 * MARGIN - 1) * scale(minLimit);
        yPoints[0] = (h - 2 * MARGIN - 1) * scale(remapDacValue(calibrationBase, configuration, unit, minLimit));

        xPoints[configuration.numPoints + 1] = (w - 2 * MARGIN - 1) * scale(maxLimit);
        yPoints[configuration.numPoints + 1] = (h - 2 * MARGIN - 1) * scale(remapDacValue(calibrationBase, configuration, unit, maxLimit));

        RectangleF clipRectangle = { 0.0f, 0.0f, 1.0f * (w - 2 * MARGIN - 1), 1.0f * (h - 2 * MARGIN - 1) };

        for (unsigned int i = 0; i < configuration.numPoints + 1; i++) {
            PointF p1 = { xPoints[i], yPoints[i] };
            PointF p2 = { xPoints[i + 1], yPoints[i + 1] };
            if (clipSegment(clipRectangle, p1, p2)) {
                if (!isNaN(p1.x) && !isNaN(p1.y) && !isNaN(p2.x) && !isNaN(p2.y)) {
                    drawAntialiasedLine(
                        x + MARGIN + (int)roundf(p1.x),
                        y + h - MARGIN - 1 - (int)roundf(p1.y),
                        x + MARGIN + (int)roundf(p2.x),
                        y + h - MARGIN - 1 - (int)roundf(p2.y)
                    );
                }
            }
        }
    }

    font::Glyph markerGlyph;
    font.getGlyph('\x80', markerGlyph);

    for (unsigned int i = 0; i < configuration.numPoints; i++) {

        if (xPoints[i + 1] >= 0.0f && xPoints[i + 1] <= 1.0f * (w - 2 * MARGIN - 1) &&
            yPoints[i + 1] >= 0.0f && yPoints[i + 1] <= 1.0f * (h - 2 * MARGIN - 1))
        {
            drawGlyph(font, markerGlyph, '\x80',
                x + MARGIN + (int)roundf(xPoints[i + 1]),
                y + h - MARGIN - 1 - (int)roundf(yPoints[i + 1]),
                ALIGN_CENTER,
                ALIGN_CENTER,
                x, y, x + w - 1, y + w - 1
            );
        }
    }

    // draw frame
    mcu::display::setColor(style->color);
    mcu::display::drawRect(x + MARGIN, y + MARGIN, x + w - MARGIN - 1, y + h - MARGIN - 1);

    // draw DAC value
    auto x1 = (w - 2 * MARGIN - 1) * scale(dacValue);
    if (x1 >= 0.0f && x1 <= 1.0f * (w - 2 * MARGIN - 1)) {
        mcu::display::setColor(COLOR_ID_YT_GRAPH_Y2);

        mcu::display::drawVLine(
            x + MARGIN + (int)roundf(x1),
            y + MARGIN + 1,
            h - 2 * MARGIN - 3
        );

        auto y1 = (h - 2 * MARGIN - 1) * scale(remapDacValue(calibrationBase, configuration, unit, dacValue));
        if (y1 >= 0.0f && y1 <= 1.0f * (h - 2 * MARGIN - 1)) {
            drawGlyph(font, markerGlyph, '\x80',
                x + MARGIN + (int)roundf(x1),
                y + h - MARGIN - 1 - (int)roundf(y1),
                ALIGN_CENTER,
                ALIGN_CENTER,
                x, y, x + w - 1, y + w - 1
            );
        }
    }
}

void drawEditorCalibrationChart(const WidgetCursor &widgetCursor) {
    drawCalibrationChart(calibration::g_editor, widgetCursor);
}

void drawViewerCalibrationChart(const WidgetCursor &widgetCursor) {
    drawCalibrationChart(calibration::g_viewer, widgetCursor);
}

////////////////////////////////////////////////////////////////////////////////

float remapDacValue(calibration::CalibrationBase &calibrationBase, CalibrationValueConfiguration &configuration, Unit unit, float value) {
    if (configuration.numPoints < 2) {
        return value;
    }

    unsigned int i;

    for (i = 1; i < configuration.numPoints - 1 && value > configuration.points[i].dac; i++) {
    }

    float remappedValue = remap(value,
            configuration.points[i - 1].dac,
            configuration.points[i - 1].value,
            configuration.points[i].dac,
            configuration.points[i].value
        );

    return calibrationBase.roundCalibrationValue(unit, remappedValue);
}

////////////////////////////////////////////////////////////////////////////////

void getCalibrationValueTypeInfo(char *text, int count) {
    auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    if (editPage) {
        editPage->getCalibrationValueTypeInfo(text, count);
    } else {
        auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        viewPage->getCalibrationValueTypeInfo(text, count);
    }
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsCalibrationEditPage::onStartPasswordOk() {
	removePageFromStack(PAGE_ID_KEYPAD);
    removePageFromStack(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);

    if (g_channel) {
        calibration::g_editor.start(g_channel->slotIndex, g_channel->subchannelIndex);
    } else {
        calibration::g_editor.start(hmi::g_selectedSlotIndex, hmi::g_selectedSubchannelIndex);
    }

    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
}

void ChSettingsCalibrationEditPage::start() {
    checkPassword("Password: ", persist_conf::devConf.calibrationPassword, onStartPasswordOk);
    onStartPasswordOk();
}

void ChSettingsCalibrationEditPage::pageAlloc() {
    m_dacValue = 0;

    if (calibration::g_editor.isEnabled()) {
        calibration::g_editor.copyValuesFromChannel();
    }

    m_version = 0;
    m_chartVersion = 0;
    m_chartZoom = 1;
    setCalibrationValueType(calibration::g_editor.getInitialCalibrationValueType());
}

void ChSettingsCalibrationEditPage::pageFree() {
    calibration::g_editor.stop();
}

int ChSettingsCalibrationEditPage::getDirty() {
    return m_version != 0;
}

void ChSettingsCalibrationEditPage::onSetRemarkOk(char *remark) {
    calibration::g_editor.setRemark(remark, strlen(remark));
    
    popPage();

    if (calibration::g_editor.save()) {
        popPage();
        infoMessage("Calibration data saved!");
    } else {
        errorMessage("Save failed!");
    }
}

void ChSettingsCalibrationEditPage::set() {
    int16_t scpiErr;
    int16_t uiErr;
    bool result = calibration::g_editor.canSave(scpiErr, &uiErr);
    if (result || uiErr == SCPI_ERROR_CALIBRATION_REMARK_NOT_SET) {
        Keypad::startPush("Remark: ", calibration::g_editor.getRemark(), 0, CALIBRATION_REMARK_MAX_LENGTH, false, onSetRemarkOk, popPage);
    } else {
        generateError(uiErr);
    }
}

void ChSettingsCalibrationEditPage::zoomChart() {
    const int MAX_ZOOM = 64;

    m_chartZoom *= 2;
    if (m_chartZoom > MAX_ZOOM) {
        m_chartZoom = 1;
    }

    m_chartVersion++;
}

bool ChSettingsCalibrationEditPage::isCalibrationValueTypeSelectable() {
    return calibration::g_editor.isCalibrationValueTypeSelectable();
}

CalibrationValueType ChSettingsCalibrationEditPage::getCalibrationValueType() {
    return m_calibrationValueType;
}

void ChSettingsCalibrationEditPage::getCalibrationValueTypeInfo(char *text, int count) {
    int slotIndex;
    int subchannelIndex;
    calibration::g_editor.getCalibrationChannel(slotIndex, subchannelIndex);

    const char *valueTypeDescription = m_calibrationValueType == CALIBRATION_VALUE_U ? "Voltage" : "Current";
    const char *rangeDescription = g_slots[slotIndex]->getCalibrationValueRangeDescription(subchannelIndex);

    if (rangeDescription) {
        snprintf(text, count, "%s [%s]", valueTypeDescription, rangeDescription);
    } else {
        snprintf(text, count, "%s", valueTypeDescription);
    }
}

void ChSettingsCalibrationEditPage::setCalibrationValueType(CalibrationValueType type) {
    m_calibrationValueType = type;

    if (calibration::g_editor.isEnabled()) {
        if (m_calibrationValueType == CALIBRATION_VALUE_I_LOW_RANGE) {
            calibration::g_editor.selectCurrentRange(1);
        } else {
            calibration::g_editor.selectCurrentRange(0);
        }

        int slotIndex;
        int subchannelIndex;
        calibration::g_editor.getCalibrationChannel(slotIndex, subchannelIndex);
        Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);
        if (channel) {
            if (m_calibrationValueType == CALIBRATION_VALUE_U) {
                channel_dispatcher::setCurrent(*channel, g_channel->params.U_CAL_I_SET);
            } else {
                if (m_calibrationValueType == CALIBRATION_VALUE_I_HI_RANGE) {
                    channel_dispatcher::setVoltage(*channel, g_channel->params.I_CAL_U_SET);
                } else {
                    channel_dispatcher::setVoltage(*channel, g_channel->params.I_LOW_RANGE_CAL_U_SET);
                }
            }
        }
    }

    auto &configuration = getCalibrationValue().configuration;
    if (configuration.numPoints > 0) {
        selectPointAtIndex(0);
    }

    m_chartVersion++;
}

float ChSettingsCalibrationEditPage::getDacValue() {
    return m_dacValue;
}

void ChSettingsCalibrationEditPage::setDacValue(float value) {
    m_dacValue = value;

    if (m_calibrationValueType == CALIBRATION_VALUE_U) {
        calibration::g_editor.getVoltage().setDacValue(value);
    } else {
        calibration::g_editor.getCurrent().setDacValue(value);
    }

    if (getCalibrationValue().measure(m_measuredValue, nullptr)) {
        m_measuredValueChanged = true;
    } else {
        m_measuredValue = remapDacValue(
            calibration::g_editor,
            getCalibrationValue().configuration,
            m_calibrationValueType == CALIBRATION_VALUE_U ? UNIT_VOLT : UNIT_AMPER,
            value
        );
        m_measuredValueChanged = false;
    }

    m_chartVersion++;
}

bool ChSettingsCalibrationEditPage::isCalibrationValueSource() {
    return calibration::g_editor.isCalibrationValueSource();
}

float ChSettingsCalibrationEditPage::getMeasuredValue() {
    return m_measuredValue;
}

float ChSettingsCalibrationEditPage::getLiveMeasuredValue() {
    float measuredValue;
    getCalibrationValue().measure(measuredValue, nullptr);
    return measuredValue;
}

void ChSettingsCalibrationEditPage::setMeasuredValue(float value) {
    m_measuredValue = value;
    m_measuredValueChanged = true;
    m_chartVersion++;
}

bool ChSettingsCalibrationEditPage::canEditMeasuredValue() {
    auto &configuration = getCalibrationValue().configuration;

    if (configuration.numPoints < calibration::g_editor.getMaxCalibrationPoints()) {
        return true;
    }

    float dac = getDacValue();

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        if (compareValues(configuration.points[i].dac, dac) == 0) {
            return true;
        }
    }

    return false;    
}

bool ChSettingsCalibrationEditPage::canMoveToPreviousPoint() {
    auto &configuration = getCalibrationValue().configuration;

    float dac = getDacValue();

    for (int8_t i = configuration.numPoints - 1; i >= 0; i--) {
        if (compareValues(configuration.points[i].dac, dac) < 0) {
            return true;
        }
    }

    return false;
}

void ChSettingsCalibrationEditPage::moveToPreviousPoint() {
    auto &configuration = getCalibrationValue().configuration;

    float dac = getDacValue();

    for (int8_t i = configuration.numPoints - 1; i >= 0; i--) {
        if (compareValues(configuration.points[i].dac, dac) < 0) {
            selectPointAtIndex(i);
            break;
        }
    }
}

bool ChSettingsCalibrationEditPage::canMoveToNextPoint() {
    auto &configuration = getCalibrationValue().configuration;

    float dac = getDacValue();

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        if (compareValues(configuration.points[i].dac, dac) > 0) {
            return true;
        }
    }

    return false;
}

void ChSettingsCalibrationEditPage::moveToNextPoint() {
    auto &configuration = getCalibrationValue().configuration;

    float dac = getDacValue();

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        if (compareValues(configuration.points[i].dac, dac) > 0) {
            selectPointAtIndex(i);
            break;
        }
    }
}

bool ChSettingsCalibrationEditPage::canEditSetValue() {
    auto &configuration = getCalibrationValue().configuration;
    return configuration.numPoints < calibration::g_editor.getMaxCalibrationPoints();
}

bool ChSettingsCalibrationEditPage::canSavePoint() {
    if (!calibration::g_editor.isEnabled()) {
        return false;
    }

    float measuredValue;
    if (getCalibrationValue().measure(measuredValue, nullptr)) {
        if (isNaN(measuredValue)) {
            return false;
        }

        if (!m_measuredValueChanged && compareValues(measuredValue, m_measuredValue) == 0) {
            return false;
        }
    } else {
        if (!m_measuredValueChanged) {
            return false;
        }

        measuredValue = m_measuredValue;
    }

    if (calibration::g_editor.isPowerChannel()) {
        if (m_calibrationValueType == CALIBRATION_VALUE_U && calibration::g_editor.getChannelMode() != CHANNEL_MODE_CV) {
            return false;
        }

        if (m_calibrationValueType != CALIBRATION_VALUE_U && calibration::g_editor.getChannelMode() != CHANNEL_MODE_CC) {
            return false;
        }
    }

    auto &configuration = getCalibrationValue().configuration;

    if (configuration.numPoints == 0) {
        return true;
    }

    float dac = getDacValue();

    unsigned int i;
    for (i = 0; i < configuration.numPoints; i++) {
        if (compareValues(configuration.points[i].dac, dac) >= 0) {
            break;
        }
    }

    if (compareValues(configuration.points[i].dac, dac) == 0) {
        return configuration.points[i].value != measuredValue;
    } else {
        if (configuration.numPoints < calibration::g_editor.getMaxCalibrationPoints()) {
            return true;
        }
    }

    return false;
}

void ChSettingsCalibrationEditPage::savePoint() {
    if (!calibration::g_editor.isEnabled()) {
        return;
    }

    float measuredValue;
    if (getCalibrationValue().measure(measuredValue, nullptr)) {
        if (isNaN(measuredValue)) {
            return;
        }

        if (!m_measuredValueChanged && compareValues(measuredValue, m_measuredValue) == 0) {
            return;
        }
    } else {
        if (!m_measuredValueChanged) {
            return;
        }

        measuredValue = m_measuredValue;
    }

    if (calibration::g_editor.isPowerChannel()) {
        if (m_calibrationValueType == CALIBRATION_VALUE_U && calibration::g_editor.getChannelMode() != CHANNEL_MODE_CV) {
            return;
        }

        if (m_calibrationValueType != CALIBRATION_VALUE_U && calibration::g_editor.getChannelMode() != CHANNEL_MODE_CC) {
            return;
        }
    }
    
    auto &calibrationValue = getCalibrationValue();
    auto &configuration = calibrationValue.configuration;

    float dac = getDacValue();
    float adc = getAdcValue();

    unsigned int i;
    for (i = 0; i < configuration.numPoints; i++) {
        if (compareValues(configuration.points[i].dac, dac) >= 0) {
            break;
        }
    }

    if (i < configuration.numPoints && compareValues(configuration.points[i].dac, dac) == 0) {
        calibrationValue.isPointSet[i] = true;
        configuration.points[i].value = measuredValue;
        configuration.points[i].adc = adc;

        m_version++;
        m_chartVersion++;

        m_measuredValue = measuredValue;
    } else {
        if (configuration.numPoints < calibration::g_editor.getMaxCalibrationPoints()) {
            for (unsigned int j = configuration.numPoints; j > i; j--) {
                calibrationValue.isPointSet[j] = calibrationValue.isPointSet[j - 1];
                configuration.points[j].dac = configuration.points[j - 1].dac;
                configuration.points[j].value = configuration.points[j - 1].value;
                configuration.points[j].adc = configuration.points[j - 1].adc;
            }

            calibrationValue.isPointSet[i] = true;
            configuration.points[i].dac = dac;
            configuration.points[i].value = measuredValue;
            configuration.points[i].adc = adc;

            configuration.numPoints++;

            m_version++;
            m_chartVersion++;

            m_measuredValue = measuredValue;
        }
    }

    m_measuredValueChanged = false;
}

bool ChSettingsCalibrationEditPage::canDeletePoint() {
    if (!calibration::g_editor.isEnabled()) {
        return false;
    }

    auto &configuration = getCalibrationValue().configuration;

    float dac = getDacValue();

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        if (compareValues(configuration.points[i].dac, dac) == 0) {
            return true;
        }
    }

    return false;
}

void ChSettingsCalibrationEditPage::deletePoint() {
    if (!calibration::g_editor.isEnabled()) {
        return;
    }

    auto &calibrationValue = getCalibrationValue();
    auto &configuration = calibrationValue.configuration;

    float dac = getDacValue();

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        if (compareValues(configuration.points[i].dac, dac) == 0) {
            for (; i < configuration.numPoints - 1; i++) {
                calibrationValue.isPointSet[i] = calibrationValue.isPointSet[i + 1];
                configuration.points[i].dac = configuration.points[i + 1].dac;
                configuration.points[i].value = configuration.points[i + 1].value;
                configuration.points[i].adc = configuration.points[i + 1].adc;
            }

            configuration.numPoints--;

            m_version++;
            m_chartVersion++;

            if (canMoveToNextPoint()) {
                moveToNextPoint();
            } else if (canMoveToPreviousPoint()) {
                moveToPreviousPoint();
            }

            break;
        }
    }
}

int ChSettingsCalibrationEditPage::getCurrentPointIndex() {
    auto &configuration = getCalibrationValue().configuration;

    float dac = getDacValue();

    for (unsigned int i = 0; i < configuration.numPoints; i++) {
        if (compareValues(configuration.points[i].dac, dac) == 0) {
            return i;
        }
    }

    return -1;
}

unsigned int ChSettingsCalibrationEditPage::getNumPoints() {
    auto &configuration = getCalibrationValue().configuration;
    return configuration.numPoints;
}

calibration::Value &ChSettingsCalibrationEditPage::getCalibrationValue() {
    if (m_calibrationValueType == CALIBRATION_VALUE_U) {
        return calibration::g_editor.getVoltage();
    } else {
        return calibration::g_editor.getCurrent();
    }
}

float ChSettingsCalibrationEditPage::getAdcValue() {
    return calibration::g_editor.getAdcValue(m_calibrationValueType);
}

int ChSettingsCalibrationEditPage::compareValues(float dac1, float dac2) {
    Unit unit = m_calibrationValueType == CALIBRATION_VALUE_U ? UNIT_VOLT : UNIT_AMPER;
    dac1 = calibration::g_editor.roundCalibrationValue(unit, dac1);
    dac2 = calibration::g_editor.roundCalibrationValue(unit, dac2);
    return dac1 > dac2 ? 1 : dac1 < dac2 ? -1 : 0;
}

void ChSettingsCalibrationEditPage::selectPointAtIndex(int8_t i) {
    auto &configuration = getCalibrationValue().configuration;
    
    m_dacValue = configuration.points[i].dac;
    m_measuredValue = configuration.points[i].value;
    m_measuredValueChanged = false;
    
    if (m_calibrationValueType == CALIBRATION_VALUE_U) {
        calibration::g_editor.setVoltage(configuration.points[i].dac);
    } else {
        calibration::g_editor.setCurrent(configuration.points[i].dac);
    }

    m_chartVersion++;
}

////////////////////////////////////////////////////////////////////////////////

void ChSettingsCalibrationViewPage::start() {
    calibration::g_viewer.start(hmi::g_selectedSlotIndex, hmi::g_selectedSubchannelIndex);
    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
}

void ChSettingsCalibrationViewPage::pageAlloc() {
    m_chartVersion = 0;
    m_chartZoom = 1;
    setCalibrationValueType(calibration::g_viewer.getInitialCalibrationValueType());
}

void ChSettingsCalibrationViewPage::zoomChart() {
    const int MAX_ZOOM = 64;

    m_chartZoom *= 2;
    if (m_chartZoom > MAX_ZOOM) {
        m_chartZoom = 1;
    }

    m_chartVersion++;
}

bool ChSettingsCalibrationViewPage::isCalibrationValueTypeSelectable() {
    return g_channel != nullptr;
}

CalibrationValueType ChSettingsCalibrationViewPage::getCalibrationValueType() {
    return m_calibrationValueType;
}

void ChSettingsCalibrationViewPage::getCalibrationValueTypeInfo(char *text, int count) {
    int slotIndex;
    int subchannelIndex;
    calibration::g_viewer.getCalibrationChannel(slotIndex, subchannelIndex);

    const char *valueTypeDescription = m_calibrationValueType == CALIBRATION_VALUE_U ? "Voltage" : "Current";
    const char *rangeDescription = g_slots[slotIndex]->getCalibrationValueRangeDescription(subchannelIndex);

    if (rangeDescription) {
        snprintf(text, count, "%s [%s]", valueTypeDescription, rangeDescription);
    } else {
        snprintf(text, count, "%s", valueTypeDescription);
    }
}

void ChSettingsCalibrationViewPage::setCalibrationValueType(CalibrationValueType type) {
    m_calibrationValueType = type;

    auto &configuration = getCalibrationValueConfiguration();
    if (configuration.numPoints > 0) {
        selectPointAtIndex(0);
    } else {
        selectPointAtIndex(-1);
    }

    m_chartVersion++;
}

bool ChSettingsCalibrationViewPage::isCalibrationValueSource() {
    return calibration::g_viewer.isCalibrationValueSource();
}

float ChSettingsCalibrationViewPage::getDacValue() {
    if (m_selectedPointIndex == -1) {
        return NAN;
    }
    return getCalibrationValueConfiguration().points[m_selectedPointIndex].dac;
}


float ChSettingsCalibrationViewPage::getMeasuredValue() {
    if (m_selectedPointIndex == -1) {
        return NAN;
    }
    return getCalibrationValueConfiguration().points[m_selectedPointIndex].value;
}

bool ChSettingsCalibrationViewPage::canMoveToPreviousPoint() {
    return m_selectedPointIndex > 0;
}

void ChSettingsCalibrationViewPage::moveToPreviousPoint() {
    if (m_selectedPointIndex > 0) {
        selectPointAtIndex(m_selectedPointIndex - 1);
    }
}

bool ChSettingsCalibrationViewPage::canMoveToNextPoint() {
    return m_selectedPointIndex < (int)getCalibrationValueConfiguration().numPoints - 1;
}

void ChSettingsCalibrationViewPage::moveToNextPoint() {
    if (m_selectedPointIndex < (int)getCalibrationValueConfiguration().numPoints - 1) {
        selectPointAtIndex(m_selectedPointIndex + 1);
    }
}

int ChSettingsCalibrationViewPage::getCurrentPointIndex() {
    return m_selectedPointIndex;
}

unsigned int ChSettingsCalibrationViewPage::getNumPoints() {
    auto &configuration = getCalibrationValueConfiguration();
    return configuration.numPoints;
}

CalibrationValueConfiguration &ChSettingsCalibrationViewPage::getCalibrationValueConfiguration() {
    int slotIndex;
    int subchannelIndex;
    calibration::g_viewer.getCalibrationChannel(slotIndex, subchannelIndex);
    Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

    if (channel) {
        memcpy(&m_calConf, &channel->cal_conf, sizeof(CalibrationConfiguration));
    } else {
        g_slots[slotIndex]->getCalibrationConfiguration(subchannelIndex, m_calConf, nullptr);
    }

    if (m_calibrationValueType == CALIBRATION_VALUE_U) {
        return m_calConf.u;
    } else if (m_calibrationValueType == CALIBRATION_VALUE_I_HI_RANGE) {
        return m_calConf.i[0];
    } else {
        return m_calConf.i[1];
    }
}

void ChSettingsCalibrationViewPage::selectPointAtIndex(int i) {
    m_selectedPointIndex = i;
    m_chartVersion++;
}

} // namespace gui
} // namespace psu

namespace gui {

using namespace eez::psu;
using namespace eez::psu::gui;

void data_channel_calibration_status(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration::g_viewer.isCalibrationExists();
    }
}

void data_channel_calibration_state(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : -1);
        if (iChannel != -1) {
            Channel &channel = Channel::get(iChannel);
            value = channel.isCalibrationEnabled();
        } else {
            value = g_slots[hmi::g_selectedSlotIndex]->isVoltageCalibrationEnabled(hmi::g_selectedSubchannelIndex);
        }
    }
}

void data_channel_calibration_date(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        uint32_t calibrationDate;

        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : -1);
        if (iChannel != -1) {
            Channel &channel = Channel::get(iChannel);
            calibrationDate = channel.cal_conf.calibrationDate;
        } else {
            g_slots[hmi::g_selectedSlotIndex]->getCalibrationDate(hmi::g_selectedSubchannelIndex, calibrationDate, nullptr);
        }

        value = Value(calibrationDate, persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_24 || persist_conf::devConf.dateTimeFormat == datetime::FORMAT_DMY_12 ? VALUE_TYPE_DATE_DMY : VALUE_TYPE_DATE_MDY);
    }
}

void data_channel_calibration_remark(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        const char *calibrationRemark = nullptr;

        int iChannel = cursor >= 0 ? cursor : (g_channel ? g_channel->channelIndex : -1);
        if (iChannel != -1) {
            Channel &channel = Channel::get(iChannel);
            calibrationRemark = channel.cal_conf.calibrationRemark;
        } else {
            g_slots[hmi::g_selectedSlotIndex]->getCalibrationRemark(hmi::g_selectedSubchannelIndex, calibrationRemark, nullptr);
        }

        if (calibrationRemark) {
            value = Value(calibrationRemark);
        }
    }
}

void data_channel_calibration_is_enabled(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration::g_editor.isEnabled();
    }
}

void data_channel_calibration_value_type_is_selectable(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            value = editPage->isCalibrationValueTypeSelectable();
        } else {
            auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
            value = viewPage->isCalibrationValueTypeSelectable();
        }
    }
}

void data_channel_calibration_value_type(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
			if (editPage->isCalibrationValueTypeSelectable()) {
				if (calibration::g_editor.hasSupportForCurrentDualRange()) {
					value = MakeEnumDefinitionValue(editPage->getCalibrationValueType(), ENUM_DEFINITION_CALIBRATION_VALUE_TYPE_DUAL_RANGE);
				} else {
					value = MakeEnumDefinitionValue(editPage->getCalibrationValueType(), ENUM_DEFINITION_CALIBRATION_VALUE_TYPE);
				}
			} else {
				value = Value(0, VALUE_TYPE_CALIBRATION_VALUE_TYPE_INFO);
			}
        } else {
			if (editPage->isCalibrationValueTypeSelectable()) {
				auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
				if (calibration::g_editor.hasSupportForCurrentDualRange()) {
					value = MakeEnumDefinitionValue(viewPage->getCalibrationValueType(), ENUM_DEFINITION_CALIBRATION_VALUE_TYPE_DUAL_RANGE);
				} else {
					value = MakeEnumDefinitionValue(viewPage->getCalibrationValueType(), ENUM_DEFINITION_CALIBRATION_VALUE_TYPE);
				}
			} else {
				value = Value(0, VALUE_TYPE_CALIBRATION_VALUE_TYPE_INFO);
			}
		}
    }
}

void data_channel_calibration_is_power_channel(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = calibration::g_editor.isPowerChannel();
    }
}

void data_channel_calibration_value_is_voltage(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        value = page->getCalibrationValueType() == CALIBRATION_VALUE_U;
    }
}

void data_calibration_point_can_edit_set_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        value = page->canEditSetValue();
    }
}

void data_calibration_point_set_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    if (editPage) {
        int slotIndex;
        int subchannelIndex;
        calibration::g_editor.getCalibrationChannel(slotIndex, subchannelIndex);
        Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

        if (operation == DATA_OPERATION_GET) {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_U_EDIT;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
                data_keypad_text(operation, cursor, value);
            } else {
                if (editPage->getCalibrationValueType() == CALIBRATION_VALUE_U) {
                    value = MakeValue(editPage->getDacValue(), UNIT_VOLT);
                } else {
                    value = MakeValue(editPage->getDacValue(), UNIT_AMPER);
                }
            }
        } else if (operation == DATA_OPERATION_GET_MIN) {
            float fvalue;
            Unit unit;
            calibration::g_editor.getMinValue(editPage->getCalibrationValueType(), fvalue, unit);
            value = MakeValue(fvalue, unit);
        } else if (operation == DATA_OPERATION_GET_MAX) {
            float fvalue;
            Unit unit;
            calibration::g_editor.getMaxValue(editPage->getCalibrationValueType(), fvalue, unit);
            value = MakeValue(fvalue, unit);
        } else if (operation == DATA_OPERATION_GET_NAME) {
            value = editPage->getCalibrationValueType() == CALIBRATION_VALUE_U ? "Voltage" : "Current";
        } else if (operation == DATA_OPERATION_GET_UNIT) {
            value = editPage->getCalibrationValueType() == CALIBRATION_VALUE_U ? UNIT_VOLT : UNIT_AMPER;
        } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
            value = channel ? 1 : 0;
        } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
            if (editPage->getCalibrationValueType() == CALIBRATION_VALUE_U) {
                channel_dispatcher::getVoltageStepValues(slotIndex, subchannelIndex, value.getStepValues(), true);
            } else {
                channel_dispatcher::getCurrentStepValues(slotIndex, subchannelIndex, value.getStepValues(), true);
            }
            value = 1;
        } else if (operation == DATA_OPERATION_SET) {
            editPage->setDacValue(value.getFloat());
        }
    } else {
        auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        if (operation == DATA_OPERATION_GET) {
            value = MakeValue(viewPage->getDacValue(), viewPage->getCalibrationValueType() == CALIBRATION_VALUE_U ? UNIT_VOLT : UNIT_AMPER);
        }
    }
}

void data_channel_is_calibration_value_source(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            value = editPage->isCalibrationValueSource();
        } else {
            auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
            value = viewPage->isCalibrationValueSource();
        }
    }
}

void data_calibration_point_measured_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    if (editPage) {
        int slotIndex;
        int subchannelIndex;
        calibration::g_editor.getCalibrationChannel(slotIndex, subchannelIndex);
        Channel *channel = Channel::getBySlotIndex(slotIndex, subchannelIndex);

        if (operation == DATA_OPERATION_GET) {
            bool focused = g_focusCursor == cursor && g_focusDataId == DATA_ID_CHANNEL_U_EDIT;
            if (focused && g_focusEditValue.getType() != VALUE_TYPE_NONE) {
                value = g_focusEditValue;
            } else if (focused && getActivePageId() == PAGE_ID_EDIT_MODE_KEYPAD && edit_mode_keypad::g_keypad->isEditing()) {
                data_keypad_text(operation, cursor, value);
            } else {
                value = MakeValue(editPage->getMeasuredValue(), editPage->getCalibrationValueType() == CALIBRATION_VALUE_U ? UNIT_VOLT : UNIT_AMPER);
            }
        } else if (operation == DATA_OPERATION_GET_MIN) {
            float fvalueMin;
            float fvalueMax;
            Unit unit;
            calibration::g_editor.getMinValue(editPage->getCalibrationValueType(), fvalueMin, unit);
            calibration::g_editor.getMaxValue(editPage->getCalibrationValueType(), fvalueMax, unit);
            value = MakeValue(roundPrec(fvalueMin - fabsf(fvalueMax - fvalueMin) * 0.1f, 1E-4f), unit);
        } else if (operation == DATA_OPERATION_GET_MAX) {
            float fvalueMin;
            float fvalueMax;
            Unit unit;
            calibration::g_editor.getMinValue(editPage->getCalibrationValueType(), fvalueMin, unit);
            calibration::g_editor.getMaxValue(editPage->getCalibrationValueType(), fvalueMax, unit);
            value = MakeValue(roundPrec(fvalueMax + fabsf(fvalueMax - fvalueMin) * 0.1f, 1E-4f), unit);
        }  else if (operation == DATA_OPERATION_GET_NAME) {
            value = editPage->getCalibrationValueType() == CALIBRATION_VALUE_U ? "Voltage" : "Current";
        } else if (operation == DATA_OPERATION_GET_UNIT) {
            value = editPage->getCalibrationValueType() == CALIBRATION_VALUE_U ? UNIT_VOLT : UNIT_AMPER;
        } else if (operation == DATA_OPERATION_GET_IS_CHANNEL_DATA) {
            value = channel ? 1 : 0;
        } else if (operation == DATA_OPERATION_GET_ENCODER_STEP_VALUES) {
            if (editPage->getCalibrationValueType() == CALIBRATION_VALUE_U) {
                channel_dispatcher::getVoltageStepValues(slotIndex, subchannelIndex, value.getStepValues(), true);
            } else {
                channel_dispatcher::getCurrentStepValues(slotIndex, subchannelIndex, value.getStepValues(), true);
            }
            value = 1;
        } else if (operation == DATA_OPERATION_SET) {
            editPage->setMeasuredValue(value.getFloat());
        }
    } else {
        auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        if (operation == DATA_OPERATION_GET) {
            value = MakeValue(viewPage->getMeasuredValue(), viewPage->getCalibrationValueType() == CALIBRATION_VALUE_U ? UNIT_VOLT : UNIT_AMPER);
        }
    }
}

void data_calibration_point_live_measured_value(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        float fValue = editPage->getLiveMeasuredValue();
        if (isNaN(fValue)) {
            value = "";
        } else {
            value = MakeValue(fValue, editPage->getCalibrationValueType() == CALIBRATION_VALUE_U ? UNIT_VOLT : UNIT_AMPER);
        }
    }
}

void data_channel_calibration_point_can_move_to_previous(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            value = editPage->canMoveToPreviousPoint();
        } else {
            auto editPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
            value = editPage->canMoveToPreviousPoint();
        }
    }
}

void data_channel_calibration_point_can_move_to_next(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            value = editPage->canMoveToNextPoint();
        } else {
            auto editPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
            value = editPage->canMoveToNextPoint();
        }
    }
}

void data_channel_calibration_point_can_save(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        value = page->canSavePoint();
    }
}

void data_channel_calibration_point_can_delete(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto page = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        value = page->canDeletePoint();
    }
}

Value MakeCalibrationPointInfoValue(int8_t currentPointIndex, int8_t numPoints) {
    Value value;
    value.type_ = VALUE_TYPE_CALIBRATION_POINT_INFO;
    value.pairOfInt16_.first = currentPointIndex;
    value.pairOfInt16_.second = numPoints;
    return value;
}

void data_channel_calibration_point_info(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            value = MakeCalibrationPointInfoValue(editPage->getCurrentPointIndex(), editPage->getNumPoints());
        } else {
            auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
            value = MakeCalibrationPointInfoValue(viewPage->getCurrentPointIndex(), viewPage->getNumPoints());
        }
    }
}

void data_channel_calibration_chart(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            value = editPage->getChartVersion();
        } else {
            auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
            value = viewPage->getChartVersion();
        }
    } else if (operation == DATA_OPERATION_GET_CANVAS_DRAW_FUNCTION) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            value = Value((void *)drawEditorCalibrationChart, VALUE_TYPE_POINTER);
        } else {
            value = Value((void *)drawViewerCalibrationChart, VALUE_TYPE_POINTER);
        }
    }
}

void data_channel_calibration_chart_zoom(DataOperationEnum operation, Cursor cursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
        if (editPage) {
            value = Value(editPage->getChartZoom(), VALUE_TYPE_ZOOM);
        } else {
            auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
            value = Value(viewPage->getChartZoom(), VALUE_TYPE_ZOOM);
        }
    }
}

void action_ch_settings_calibration_start_calibration() {
    ChSettingsCalibrationEditPage::start();
}

void action_ch_settings_calibration_view_points() {
    ChSettingsCalibrationViewPage::start();
}

void onSetChannelCalibrationValueType(uint16_t value) {
    popPage();

    auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    if (editPage) {
        editPage->setCalibrationValueType((CalibrationValueType)value);
    } else {
        auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        if (viewPage) {
            viewPage->setCalibrationValueType((CalibrationValueType)value);
        }
    }
}

void action_select_channel_calibration_value_type() {
    calibration::CalibrationBase *calibrationBase;

    if (getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT)) {
        calibrationBase = &calibration::g_editor;
    } else {
        calibrationBase = &calibration::g_viewer;
    }

    if (calibrationBase->hasSupportForCurrentDualRange()) {
        pushSelectFromEnumPage(ENUM_DEFINITION_CALIBRATION_VALUE_TYPE_DUAL_RANGE, calibrationBase->getCalibrationValueType(), nullptr, onSetChannelCalibrationValueType);
    } else {
        pushSelectFromEnumPage(ENUM_DEFINITION_CALIBRATION_VALUE_TYPE, calibrationBase->getCalibrationValueType(), nullptr, onSetChannelCalibrationValueType);
    }
}

void action_channel_calibration_point_previous() {
    auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    if (editPage) {
        editPage->moveToPreviousPoint();
    } else {
        auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        viewPage->moveToPreviousPoint();
    }
}

void action_channel_calibration_point_next() {
    auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    if (editPage) {
        editPage->moveToNextPoint();
    } else {
        auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        viewPage->moveToNextPoint();
    }
}

void action_channel_calibration_point_save() {
    auto page = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    page->savePoint();
}

void action_channel_calibration_point_delete() {
    auto page = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    page->deletePoint();
}

void action_channel_calibration_chart_zoom() {
    auto editPage = (ChSettingsCalibrationEditPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_EDIT);
    if (editPage) {
        editPage->zoomChart();
    } else {
        auto viewPage = (ChSettingsCalibrationViewPage *)getPage(PAGE_ID_CH_SETTINGS_CALIBRATION_VIEW);
        viewPage->zoomChart();
    }
}

void action_show_ch_settings_cal() {
    calibration::g_viewer.start(hmi::g_selectedSlotIndex, hmi::g_selectedSubchannelIndex);
    pushPage(PAGE_ID_CH_SETTINGS_CALIBRATION);
}

} // namespace gui

} // namespace eez

#endif
