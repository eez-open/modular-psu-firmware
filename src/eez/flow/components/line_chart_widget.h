/*
* EEZ Generic Firmware
* Copyright (C) 2021-present, Envox d.o.o.
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

#include <eez/flow/private.h>

namespace eez {
namespace flow {

struct LineChartLine {
    AssetsPtr<uint8_t> label;
    uint16_t color;
    uint16_t reserved;
    AssetsPtr<uint8_t> value;
};

struct LineChartWidgetComponenent : public Component {
    uint32_t maxPoints;
    AssetsPtr<uint8_t> xValue;
    ListOfAssetsPtr<LineChartLine> lines;
};

#if OPTION_GUI || !defined(OPTION_GUI)

struct Point {
    float x;
    float lines[1];
};

struct LineChartWidgetComponenentExecutionState : public ComponenentExecutionState {
    LineChartWidgetComponenentExecutionState();
    ~LineChartWidgetComponenentExecutionState();

    void init(uint32_t numLines, uint32_t maxPoints);

    uint32_t numLines;
    uint32_t maxPoints;
    uint32_t numPoints;
    uint32_t startPointIndex;

    Value *lineLabels;

    bool updated;

    Value getX(int pointIndex);
    void setX(int pointIndex, Value& value);

    float getY(int pointIndex, int lineIndex);
    void setY(int pointIndex, int lineIndex, float value);

private:
    // Data structure where n is no. of points, m is no. of lines, Xi is Value and Yij is float:
    // X1
    // X2
    // Xn
    // Y11 Y12 ... Y1m
    // Y21 Y22 ... Y2m
    // ...
    // Yn1 Yn2 ... Ynm,
    void *data;
};

#endif // OPTION_GUI || !defined(OPTION_GUI)

} // flow
} // eez
