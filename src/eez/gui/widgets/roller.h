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

namespace eez {
namespace gui {

struct RollerWidget : public Widget {
	int16_t min;
	int16_t max;
    int16_t text;
    int16_t selectedValueStyle;
    int16_t unselectedValueStyle;
    uint16_t componentIndex;
};

struct RollerWidgetState : public WidgetState {
    Value data;

	int minValue = 1;
	int maxValue = 100;

    int textHeight;

    bool isDragging = false;
    bool isRunning = false;

    float position = 0;
    float velocity = 0;

    int dragOrigin = 0;
    int dragStartPosition = 0;
    int dragPosition = 0;

    bool updateState() override;
    void render() override;

    bool hasOnTouch() override;
	void onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) override;

private:
    void updatePosition();
    bool isMoving();
    void applyDragForce();
    void applySnapForce();
    void applyEdgeForce();
    void applyForce(float force);
};


} // namespace gui
} // namespace eez
