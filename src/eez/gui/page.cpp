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

#include <eez/gui/page.h>

#include <eez/util.h>

#include <eez/gui/draw.h>
#include <eez/gui/gui.h>
#include <eez/gui/app_context.h>
#include <eez/modules/mcu/display.h>

using namespace eez::mcu;

namespace eez {
namespace gui {

////////////////////////////////////////////////////////////////////////////////

void Page::pageAlloc() {
}

void Page::pageFree() {
}

void Page::pageWillAppear() {
}

bool Page::onEncoder(int counter) {
    return false;
}

bool Page::onEncoderClicked() {
    return false;
}

Unit Page::getEncoderUnit() {
    return UNIT_UNKNOWN;
}

int Page::getDirty() {
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

void SetPage::edit() {
}

void SetPage::onSetValue(float value) {
    popPage();
    SetPage *page = (SetPage *)getActivePage();
    page->setValue(value);
}

void SetPage::setValue(float value) {
}

void SetPage::discard() {
    popPage();
}

////////////////////////////////////////////////////////////////////////////////

static ToastMessagePage g_toastMessagePage;

ToastMessagePage *ToastMessagePage::findFreePage() {
    ToastMessagePage *page = &g_toastMessagePage;

    if (page->appContext && page->appContext->getActivePage() == page) {
        page->appContext->popPage();
    }

    return page;
}

void ToastMessagePage::pageFree() {
    appContext = nullptr;
}

ToastMessagePage *ToastMessagePage::create(ToastType type, const char *message1) {
    ToastMessagePage *page = ToastMessagePage::findFreePage();
    
    page->type = type;
    page->message1 = message1;
    page->message2 = nullptr;
    page->message3 = nullptr;

    page->actionWidget.action = 0;
    page->appContext = g_appContext;

    return page;
}

ToastMessagePage *ToastMessagePage::create(ToastType type, data::Value message1Value)  {
    ToastMessagePage *page = ToastMessagePage::findFreePage();

    page->type = type;
    page->message1 = nullptr;
    page->message1Value = message1Value;
    page->message2 = nullptr;
    page->message3 = nullptr;

    page->actionWidget.action = 0;
    page->appContext = g_appContext;

    return page;
}

ToastMessagePage *ToastMessagePage::create(ToastType type, const char *message1, const char *message2)  {
    ToastMessagePage *page = ToastMessagePage::findFreePage();

    page->type = type;
    page->message1 = message1;
    page->message2 = message2;
    page->message3 = nullptr;

    page->actionWidget.action = 0;
    page->appContext = g_appContext;

    return page;
}

ToastMessagePage *ToastMessagePage::create(ToastType type, const char *message1, const char *message2, const char *message3)  {
    ToastMessagePage *page = ToastMessagePage::findFreePage();

    page->type = type;
    page->message1 = message1;
    page->message2 = message2;
    page->message3 = message3;

    page->actionWidget.action = 0;
    page->appContext = g_appContext;

    return page;
}

ToastMessagePage *ToastMessagePage::create(ToastType type, data::Value message1Value, void (*action)(int param), const char *actionLabel, int actionParam) {
    ToastMessagePage *page = ToastMessagePage::findFreePage();

    page->type = type;
    page->message1 = nullptr;
    page->message1Value = message1Value;
    page->message2 = actionLabel;
    page->message3 = nullptr;
    page->actionWidgetIsActive = false;

    page->actionWidget.action = ACTION_ID_INTERNAL_TOAST_ACTION;
    page->appContext = g_appContext;
    page->appContext->m_toastAction = action;
    page->appContext->m_toastActionParam = actionParam;

    return page;
}

bool ToastMessagePage::onEncoder(int counter) {
    popPage();
    return false;
}

bool ToastMessagePage::onEncoderClicked() {
    popPage();
    return false;
}

void ToastMessagePage::refresh() {
    const Style *style = getStyle(type == INFO_TOAST ? STYLE_ID_INFO_ALERT : STYLE_ID_ERROR_ALERT);

    font::Font font = styleGetFont(style);

    const char *message1 = this->message1;
    char message1TextBuffer[256];
    if (message1 == nullptr) {
        message1Value.toText(message1TextBuffer, sizeof(message1TextBuffer));
        message1 = message1TextBuffer;
    }

    int minTextWidth = 80;
    int textWidth1 = display::measureStr(message1, -1, font, 0);
    int textWidth2 = message2 ? display::measureStr(message2, -1, font, 0) : 0;
    int textWidth3 = message3 ? display::measureStr(message3, -1, font, 0) : 0;
    int textWidth = MAX(minTextWidth, MAX(MAX(textWidth1, textWidth2), textWidth3));
    int textHeight = font.getHeight();

    width = style->border_size_left + style->padding_left +
        textWidth +
        style->padding_right + style->border_size_right;

    height = style->border_size_top + style->padding_top +
        ((message3 || actionWidget.action) ? 3 : message2 ? 2 : 1) * textHeight +
        style->padding_bottom + style->border_size_bottom;

	x = appContext->x + (appContext->width - width) / 2;
	y = appContext->y + (appContext->height - height) / 2;

    int x1 = x;
    int y1 = y;
    int x2 = x + width - 1;
    int y2 = y + height - 1;

    int borderRadius = style->border_radius;
    if (style->border_size_top > 0 || style->border_size_right > 0 || style->border_size_bottom > 0 || style->border_size_left > 0) {
        display::setColor(style->border_color);
        if ((style->border_size_top == 1 && style->border_size_right == 1 && style->border_size_bottom == 1 && style->border_size_left == 1) && borderRadius == 0) {
            display::drawRect(x, y, x2, y2);
        } else {
            display::fillRect(x1, y1, x2, y2, style->border_radius);
			borderRadius = MAX(borderRadius - MAX(style->border_size_top, MAX(style->border_size_right, MAX(style->border_size_bottom, style->border_size_left))), 0);
        }
        x1 += style->border_size_left;
        y1 += style->border_size_top;
        x2 -= style->border_size_right;
        y2 -= style->border_size_bottom;
    }

    // draw background
    display::setColor(style->background_color);
    display::fillRect(x1, y1, x2, y2, borderRadius);

    // draw text message
    display::setColor(style->color);

    display::drawStr(message1, -1, 
        x1 + style->padding_left + (textWidth - textWidth1) / 2, 
        y1 + style->padding_top, 
        x1, y1, x2, y2, font);

    if (message2) {
        if (actionWidget.action) {
            const Style *activeStyle = getStyle(STYLE_ID_ERROR_ALERT_BUTTON);
            
            actionWidget.x = x1 + style->padding_left;
            actionWidget.y = y1 + style->padding_top + textHeight + style->padding_top;
            actionWidget.w = x2 - x1 - style->padding_left - style->padding_right;
            actionWidget.h = textHeight;
            
            if (actionWidgetIsActive) {
                display::setColor(activeStyle->color);

                display::fillRect(
                    actionWidget.x,
                    actionWidget.y,
                    actionWidget.x + actionWidget.w - 1,
                    actionWidget.y + actionWidget.h - 1,
                    0);

                display::setBackColor(activeStyle->color);
                display::setColor(activeStyle->background_color);
            } else {
                display::setBackColor(activeStyle->background_color);
                display::setColor(activeStyle->color);
            }

            display::drawStr(message2, -1, 
                actionWidget.x + (textWidth - textWidth2) / 2, actionWidget.y, 
                x1, y1, x2, y2, font);
        }
        else {
            display::drawStr(message2, -1,
                x1 + style->padding_left + (textWidth - textWidth2) / 2,
                y1 + style->padding_top + textHeight,
                x1, y1, x2, y2, font);
        }
    }

    if (message3) {
        display::drawStr(message3, -1, 
            x1 + style->padding_left + (textWidth - textWidth2) / 2, 
            y1 + style->padding_top + 2 * textHeight, 
            x1, y1, x2, y2, font);
    }
}

void ToastMessagePage::updatePage() {
    if (actionWidgetIsActive != isActiveWidget(WidgetCursor(appContext, &actionWidget, actionWidget.x, actionWidget.y, -1, 0, 0))) {
        actionWidgetIsActive = !actionWidgetIsActive;
        refresh();
    }
}

WidgetCursor ToastMessagePage::findWidget(int x, int y) {
    if (x >= this->x && x < this->x + width && y >= this->y && y < this->y + height) {
        if (actionWidget.action && x >= actionWidget.x && x < actionWidget.x + actionWidget.w && y >= actionWidget.y && y < actionWidget.y + actionWidget.h) {
            return WidgetCursor(appContext, &actionWidget, actionWidget.x, actionWidget.y, -1, 0, 0);
        }
        widget.action = ACTION_ID_INTERNAL_DIALOG_CLOSE;
        return WidgetCursor(appContext, &widget, x, y, -1, 0, 0);
    }
    
    return WidgetCursor();
}

void ToastMessagePage::executeAction() {
    popPage();
    g_appContext->m_toastAction(g_appContext->m_toastActionParam);
}

////////////////////////////////////////////////////////////////////////////////

void SelectFromEnumPage::init(const data::EnumItem *enumDefinition_, uint16_t currentValue_,
		bool (*disabledCallback_)(uint16_t value), void (*onSet_)(uint16_t))
{
	enumDefinition = enumDefinition_;
	enumDefinitionFunc = NULL;
	currentValue = currentValue_;
	disabledCallback = disabledCallback_;
	onSet = onSet_;

	init();
}

void SelectFromEnumPage::init(void (*enumDefinitionFunc_)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value),
    uint16_t currentValue_, bool (*disabledCallback_)(uint16_t value), void (*onSet_)(uint16_t))
{
	enumDefinition = NULL;
	enumDefinitionFunc = enumDefinitionFunc_;
	currentValue = currentValue_;
	disabledCallback = disabledCallback_;
	onSet = onSet_;

    init();
}

void SelectFromEnumPage::init() {
    const Style *containerStyle = getStyle(STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER);
    const Style *itemStyle = getStyle(STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM);

    font::Font font = styleGetFont(itemStyle);

    // calculate geometry
    itemHeight = itemStyle->padding_left + font.getHeight() + itemStyle->padding_right;
    itemWidth = 0;

    int i;

    char text[64];

    for (i = 0; getLabel(i); ++i) {
        getItemLabel(i, text, sizeof(text));
        int width = display::measureStr(text, -1, font);
        if (width > itemWidth) {
            itemWidth = width;
        }
    }

    numItems = i;

    itemWidth = itemStyle->padding_left + itemWidth + itemStyle->padding_right;

    width = containerStyle->padding_left + itemWidth + containerStyle->padding_right;
    if (width > display::getDisplayWidth()) {
        width = display::getDisplayWidth();
    }

    height =
        containerStyle->padding_top + numItems * itemHeight + containerStyle->padding_bottom;
    if (height > display::getDisplayHeight()) {
        height = display::getDisplayHeight();
    }

    findPagePosition();
}

uint16_t SelectFromEnumPage::getValue(int i) {
    if (enumDefinitionFunc) {
        data::Value value;
        data::Cursor cursor(i);
        enumDefinitionFunc(data::DATA_OPERATION_GET_VALUE, cursor, value);
        return value.getUInt8();
    }
    
    return enumDefinition[i].value;
}

const char *SelectFromEnumPage::getLabel(int i) {
    if (enumDefinitionFunc) {
        data::Value value;
        data::Cursor cursor(i);
        enumDefinitionFunc(data::DATA_OPERATION_GET_LABEL, cursor, value);
        return value.getString();
    }

    return enumDefinition[i].menuLabel;
}

bool SelectFromEnumPage::isDisabled(int i) {
    return disabledCallback && disabledCallback(getValue(i));
}

void SelectFromEnumPage::findPagePosition() {
	const WidgetCursor &widgetCursorAtTouchDown = getFoundWidgetAtDown();
	x = widgetCursorAtTouchDown.x;
    int right = g_appContext->x + g_appContext->width - 22;
    if (x + width > right) {
        x = right - width;
	}

    y = widgetCursorAtTouchDown.y + widgetCursorAtTouchDown.widget->h;
    int bottom = g_appContext->y + g_appContext->height - 30;
    if (y + height > bottom) {
        y = bottom - height;
	}
}

void SelectFromEnumPage::refresh() {
    const Style *containerStyle = getStyle(STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER);
	const Style *itemStyle = getStyle(STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM);
	const Style *disabledItemStyle = getStyle(STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM);

    // draw background
    display::setColor(containerStyle->background_color);
    display::fillRect(x, y, x + width - 1, y + height - 1);

    // draw labels
    char text[64];
    for (int i = 0; getLabel(i); ++i) {
        int xItem, yItem;
        getItemPosition(i, xItem, yItem);

        getItemLabel(i, text, sizeof(text));
        drawText(text, -1, xItem, yItem, itemWidth, itemHeight,
                 isDisabled(i) ? disabledItemStyle : itemStyle, nullptr, false, false, false,
                 nullptr);
    }

    dirty = false;
}

void SelectFromEnumPage::updatePage() {
    if (dirty) {
        refresh();
    }
}

WidgetCursor SelectFromEnumPage::findWidget(int x, int y) {
    int i;

    for (i = 0; getLabel(i); ++i) {
        int xItem, yItem;
        getItemPosition(i, xItem, yItem);
        if (!isDisabled(i)) {
        	if (x >= xItem && x < xItem + itemWidth && y >= yItem && y < yItem + itemHeight) {
                currentValue = getValue(i);
                dirty = true;

        		widget.action = ACTION_ID_INTERNAL_SELECT_ENUM_ITEM;
        		widget.data = (uint16_t)i;
        		return WidgetCursor(g_appContext, &widget, x, y, -1, 0, 0);
        	}
        }
    }

    return WidgetCursor();
}

void SelectFromEnumPage::selectEnumItem() {
    int itemIndex = getFoundWidgetAtDown().widget->data;
    onSet(getValue(itemIndex));
}

void SelectFromEnumPage::getItemPosition(int itemIndex, int &xItem, int &yItem) {
    const Style *containerStyle = getStyle(STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER);

    xItem = x + containerStyle->padding_left;
    yItem = y + containerStyle->padding_top + itemIndex * itemHeight;
}

void SelectFromEnumPage::getItemLabel(int itemIndex, char *text, int count) {
    if (getValue(itemIndex) == currentValue) {
        text[0] = (char)142;
    } else {
        text[0] = (char)141;
    }

    text[1] = ' ';

    strncpy(text + 2, getLabel(itemIndex), count - 3);

    text[count - 1] = 0;
}

} // namespace gui
} // namespace eez

#endif