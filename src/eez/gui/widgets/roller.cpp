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

#include <stdio.h>

#include <eez/core/util.h>
#include <eez/core/sound.h>

#include <eez/gui/gui.h>
#include <eez/gui/widgets/roller.h>

#include <eez/flow/components/roller_widget.h>

static const float FRICTION = 0.1f;
static const float BOUNCE_FORCE = 0.1f;

namespace eez {
namespace gui {

bool RollerWidgetState::updateState() {
    WIDGET_STATE_START(RollerWidget);

	const Style* selectedValueStyle = getStyle(widget->selectedValueStyle);
	font::Font fontSelectedValue = styleGetFont(selectedValueStyle);
	if (!fontSelectedValue) {
		return true;
	}

	textHeight = selectedValueStyle->paddingTop + fontSelectedValue.getHeight() + selectedValueStyle->paddingBottom;

    minValue = get(widgetCursor, widget->min).toInt32();
    maxValue = get(widgetCursor, widget->max).toInt32();

    Value dataValue = get(widgetCursor, widget->data);

	if (widgetCursor.flowState) {
		auto executionState = (flow::RollerWidgetComponenentExecutionState *)widgetCursor.flowState->componenentExecutionStates[widget->componentIndex];
		if (executionState && executionState->clear) {
			executionState->clear = false;
			isRunning = true;
			velocity = 0;
			applyForce(-1.5f * position * FRICTION);
		}
	}

    float oldPosition = position;

    if (isRunning) {
        updatePosition();

		auto newDataValue = minValue + int(roundf(-position / textHeight));
		if (newDataValue < minValue) {
			newDataValue = minValue;
		} else if (newDataValue > maxValue) {
			newDataValue = maxValue;
		}

		if (newDataValue != dataValue.getInt()) {
			dataValue = newDataValue;
			set(widgetCursor, widget->data, data);
			sound::playClick();
		}

        if (!isMoving()) {
            isRunning = false;
        }
    } else {
        position = -(dataValue.getInt() - minValue) * textHeight;
    }

    if (oldPosition != position) {
        hasPreviousState = false;
    }

	WIDGET_STATE(data, dataValue);

    WIDGET_STATE_END()
}

void RollerWidgetState::render() {
    const WidgetCursor &widgetCursor = g_widgetCursor;
    auto widget = (const RollerWidget *)widgetCursor.widget;
    const Style* style = getStyle(widget->style);
    const Style* selectedValueStyle = getStyle(widget->selectedValueStyle);
    const Style* unselectedValueStyle = getStyle(widget->unselectedValueStyle);

    font::Font fontSelectedValue = styleGetFont(selectedValueStyle);
	if (!fontSelectedValue) {
		return;
	}

	font::Font fontUnselectedValue = styleGetFont(unselectedValueStyle);
	if (!fontUnselectedValue) {
		return;
	}

	drawRectangle(
        widgetCursor.x, widgetCursor.y, widgetCursor.w, widgetCursor.h,
        style
    );

	display::AggDrawing aggDrawing;
	display::aggInit(aggDrawing);

	auto x = widgetCursor.x;

	int rectHeight = selectedValueStyle->borderSizeTop + textHeight + selectedValueStyle->borderSizeBottom;

	display::setColor(selectedValueStyle->borderColor);
	auto yCenter = widgetCursor.y + (widgetCursor.h - rectHeight) / 2;
	display::drawRoundedRect(
		aggDrawing,
		x, yCenter, x + widgetCursor.w - 1, yCenter + rectHeight - 1, selectedValueStyle->borderSizeLeft,
		selectedValueStyle->borderRadiusTLX, selectedValueStyle->borderRadiusTLY, selectedValueStyle->borderRadiusTRX, selectedValueStyle->borderRadiusTRY,
		selectedValueStyle->borderRadiusBRX, selectedValueStyle->borderRadiusBRY, selectedValueStyle->borderRadiusBLX, selectedValueStyle->borderRadiusBLY
	);

	int clip1_x1 = widgetCursor.x;
	int clip1_y1 = widgetCursor.y;
	int clip1_x2 = widgetCursor.x + widgetCursor.w - 1;
	int clip1_y2 = yCenter - 1;

	int clip2_x1 = widgetCursor.x;
	int clip2_y1 = yCenter + selectedValueStyle->borderSizeTop;
	int clip2_x2 = widgetCursor.x + widgetCursor.w - 1;
	int clip2_y2 = yCenter + rectHeight - selectedValueStyle->borderSizeBottom;

	int clip3_x1 = widgetCursor.x;
	int clip3_y1 = yCenter + rectHeight;
	int clip3_x2 = widgetCursor.x + widgetCursor.w - 1;
	int clip3_y2 = widgetCursor.y + widgetCursor.h - 1;

    auto y = widgetCursor.y + (widgetCursor.h - textHeight) / 2 + (int)roundf(position);

	display::setColor(unselectedValueStyle->color);

    for (int i = minValue; i <= maxValue; i++, y += textHeight) {
        if (y + textHeight <= widgetCursor.y) {
            continue;
        }
        if (y >= widgetCursor.y + widgetCursor.h) {
            break;
        }

        set(widgetCursor, widget->data, i);

        Value textValue = get(widgetCursor, widget->text);

        char text[100];
        textValue.toText(text, sizeof(text));

		auto textLength = strlen(text);

		int textWidth = display::measureStr(text, textLength, fontUnselectedValue, 0);
		if (y < clip1_y2) {
			display::drawStr(
				text, textLength,
				x + (widgetCursor.w - textWidth) / 2, y,
				clip1_x1, clip1_y1, clip1_x2, clip1_y2,
				fontUnselectedValue,
				-1
			);
		} else {
			display::drawStr(
				text, textLength,
				x + (widgetCursor.w - textWidth) / 2, y,
				clip3_x1, clip3_y1, clip3_x2, clip3_y2,
				fontUnselectedValue,
				-1
			);
		}

		if (y + textHeight >= clip2_y1 || y < clip2_y2) {
			int textWidth = display::measureStr(text, textLength, fontSelectedValue, 0);

			display::setColor(selectedValueStyle->color);
			display::drawStr(
				text, textLength,
				x + (widgetCursor.w - textWidth) / 2, y,
				clip2_x1, clip2_y1, clip2_x2, clip2_y2,
				fontSelectedValue,
				-1
			);
			display::setColor(unselectedValueStyle->color);
		}
    }

    set(widgetCursor, widget->data, data);
}

bool RollerWidgetState::hasOnTouch() {
    return true;
}

void RollerWidgetState::onTouch(const WidgetCursor &widgetCursor, Event &touchEvent) {
    if (touchEvent.type == EVENT_TYPE_TOUCH_DOWN) {
        dragOrigin = touchEvent.y;
        dragStartPosition = position;
        dragPosition = dragStartPosition;
    } else if (touchEvent.type == EVENT_TYPE_TOUCH_MOVE) {
		if (abs(touchEvent.y - dragOrigin) > (int)(DISPLAY_HEIGHT / 25)) {
			isDragging = true;
			isRunning = true;
			dragPosition = dragStartPosition + (touchEvent.y - dragOrigin);
		}
	} else if (touchEvent.type == EVENT_TYPE_TOUCH_UP || touchEvent.type == EVENT_TYPE_AUTO_REPEAT) {
		if (!isDragging) {
			isRunning = true;
			if (touchEvent.y < widgetCursor.y + (widgetCursor.h - textHeight) / 2) {
				applyForce(textHeight * FRICTION);
			} else if (touchEvent.y > widgetCursor.y + (widgetCursor.h - textHeight) / 2 + textHeight) {
				applyForce(-textHeight * FRICTION);
			}
		} else {
			if (touchEvent.type == EVENT_TYPE_TOUCH_UP) {
				isDragging = false;
			}
		}
	}
}

void RollerWidgetState::updatePosition() {
    applyDragForce();
    applySnapForce();
    applyEdgeForce();

    auto inverseFriction = 1.0f - FRICTION;
    velocity *= inverseFriction;
    position += velocity;
}

bool RollerWidgetState::isMoving() {
    return isDragging || fabs(velocity) >= 0.01f;
}

void RollerWidgetState::applyDragForce() {
    if (!isDragging) {
        return;
    }
    auto dragVelocity = dragPosition - position;
    applyForce(dragVelocity - velocity);
}

void RollerWidgetState::applySnapForce() {
    if (isDragging) {
        return;
    }

    // Make sure position ends at multiply of textHeight
    auto restPosition = position + velocity / FRICTION;
    auto targetRestPosition = roundf(restPosition / textHeight) * textHeight;
    applyForce(FRICTION * (targetRestPosition - position) - velocity);
}

void RollerWidgetState::applyEdgeForce() {
    if (isDragging) {
        return;
    }

    float edgeFrom = -(maxValue - minValue) * textHeight;
    float edgeTo = 0;

    if (position > edgeTo) {
        auto distanceToEdge = edgeTo - position;
        auto force = distanceToEdge * BOUNCE_FORCE;
        auto restPosition = position + (velocity + force) / FRICTION;
        if (restPosition <= edgeTo) {
            force -= velocity;
        }
        applyForce(force);
    } else if (position < edgeFrom) {
        auto distanceToEdge = edgeFrom - position;
        auto force = distanceToEdge * BOUNCE_FORCE;
        auto restPosition = position + (velocity + force) / FRICTION;
        if (restPosition >= edgeFrom) {
            force -= velocity;
        }
        applyForce(force);
    }
}

void RollerWidgetState::applyForce(float force) {
    velocity += force;
}

} // namespace gui
} // namespace eez
