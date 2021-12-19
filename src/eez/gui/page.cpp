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

#include <eez/gui/gui.h>

namespace eez {
namespace gui {

Value g_alertMessage;

void data_alert_message(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value) {
    if (operation == DATA_OPERATION_GET) {
        value = g_alertMessage;
    } else if (operation == DATA_OPERATION_SET) {
        g_alertMessage = value;
    }
}

////////////////////////////////////////////////////////////////////////////////

void Page::pageAlloc() {
}

void Page::pageFree() {
}

void Page::pageWillAppear() {
}

void Page::onEncoder(int counter) {
}

void Page::onEncoderClicked() {
}

Unit Page::getEncoderUnit() {
    return UNIT_UNKNOWN;
}

int Page::getDirty() {
    return 0;
}

void Page::set() {
}

void Page::discard() {
    getFoundWidgetAtDown().appContext->popPage();
}

bool Page::showAreYouSureOnDiscard() {
    return true;
}

////////////////////////////////////////////////////////////////////////////////

void SetPage::edit() {
}

void SetPage::onSetValue(float value) {
    getFoundWidgetAtDown().appContext->popPage();
    SetPage *page = (SetPage *)getFoundWidgetAtDown().appContext->getActivePage();
    page->setValue(value);
}

void SetPage::setValue(float value) {
}

////////////////////////////////////////////////////////////////////////////////

bool InternalPage::canClickPassThrough() {
    return false;
}

bool InternalPage::closeIfTouchedOutside() {
    return true;
}

static ToastMessagePage g_toastMessagePage;

ToastMessagePage *ToastMessagePage::findFreePage() {
    return &g_toastMessagePage;
}

void ToastMessagePage::pageFree() {
    appContext = nullptr;
}

////////////////////////////////////////

ToastMessagePage *ToastMessagePage::create(AppContext *appContext, ToastType type, const char *message, bool autoDismiss) {
    ToastMessagePage *page = ToastMessagePage::findFreePage();
    
    page->actionLabel = type == ERROR_TOAST && !autoDismiss ? "Close" : nullptr;
    page->actionWidget.action = type == ERROR_TOAST && !autoDismiss ? ACTION_ID_INTERNAL_TOAST_ACTION_WITHOUT_PARAM : 0;
    page->actionWithoutParam = nullptr;

    page->init(appContext, type, Value(message));

    return page;
}

ToastMessagePage *ToastMessagePage::create(AppContext *appContext, ToastType type, Value message)  {
    ToastMessagePage *page = ToastMessagePage::findFreePage();

    page->actionLabel = type == ERROR_TOAST ? "Close" : nullptr;
    page->actionWidget.action = type == ERROR_TOAST ? ACTION_ID_INTERNAL_TOAST_ACTION_WITHOUT_PARAM : 0;
    page->actionWithoutParam = nullptr;

    page->init(appContext, type, message);

    return page;
}

ToastMessagePage *ToastMessagePage::create(AppContext *appContext, ToastType type, Value message, void (*action)(int param), const char *actionLabel, int actionParam) {
    ToastMessagePage *page = ToastMessagePage::findFreePage();

    page->actionLabel = actionLabel;
    page->actionWidget.action = ACTION_ID_INTERNAL_TOAST_ACTION;
    page->action = action;
    page->actionParam = actionParam;

    page->init(appContext, type, message);

    return page;
}

ToastMessagePage *ToastMessagePage::create(AppContext *appContext, ToastType type, const char *message, void (*action)(), const char *actionLabel) {
    ToastMessagePage *page = ToastMessagePage::findFreePage();

    page->actionLabel = actionLabel;
    page->actionWidget.action = ACTION_ID_INTERNAL_TOAST_ACTION_WITHOUT_PARAM;
    page->actionWithoutParam = action;

    page->init(appContext, type, Value(message));

    return page;
}

////////////////////////////////////////

void ToastMessagePage::init(AppContext *appContext, ToastType type, const Value& message) {
    this->appContext = appContext;
    this->type = type;
    this->messageValue = message;

    lastActiveWidget = WidgetCursor();

    auto styleId = type == INFO_TOAST ? STYLE_ID_INFO_ALERT : STYLE_ID_ERROR_ALERT;
    auto style = getStyle(styleId);
    auto actionStyle = getStyle(STYLE_ID_ERROR_ALERT_BUTTON);

    font::Font font = styleGetFont(style);

    char *line1 = nullptr;
    char *line2 = nullptr;
    char *line3 = nullptr;

    message.toText(messageBuffer, sizeof(messageBuffer));

    // split message to up to three lines
    line1 = messageBuffer;
    line2 = strchr(line1, '\n');
    if (line2) {
        *line2 = 0;
        line2++;
        line3 = strchr(line2, '\n');
        if (line3) {
            *line3 = 0;
            line3++;
        }
    }

    int minTextWidth = 80;
    int line1Width = display::measureStr(line1, -1, font, 0);
    int line2Width = line2 ? display::measureStr(line2, -1, font, 0) : 0;
    int line3Width = line3 ? display::measureStr(line3, -1, font, 0) : 0;
    int actionLabelWidth = actionLabel ? (actionStyle->paddingLeft + display::measureStr(actionLabel, -1, font, 0) + actionStyle->paddingRight) : 0;
    int textWidth = MAX(MAX(MAX(MAX(minTextWidth, line1Width), line2Width), line3Width), actionLabelWidth);
  
    int textHeight = font.getHeight();

    width = style->borderSizeLeft + style->paddingLeft +
        textWidth +
        style->paddingRight + style->borderSizeRight;

    int numLines = (line3 ? 3 : line2 ? 2 : 1);

	auto actionLabelHeight = actionStyle->paddingTop + textHeight + actionStyle->paddingBottom;

    height = style->borderSizeTop + style->paddingTop +
        numLines * textHeight + (actionLabel ? (style->paddingTop + actionLabelHeight) : 0) +
        style->paddingBottom + style->borderSizeBottom;

    containerRectangleWidget.type = WIDGET_TYPE_RECTANGLE;
    containerRectangleWidget.data = DATA_ID_NONE;
    containerRectangleWidget.action = ACTION_ID_NONE;
    containerRectangleWidget.style = styleId;
    containerRectangleWidget.flags.ignoreLuminosity = 0;
	containerRectangleWidget.x = 0;
	containerRectangleWidget.y = 0;
	containerRectangleWidget.w = width;
	containerRectangleWidget.h = height;

    int yText = style->paddingTop;

    line2Widget.type = WIDGET_TYPE_TEXT;
    line1Widget.data = DATA_ID_NONE;
    line1Widget.action = ACTION_ID_NONE;
    line1Widget.style = styleId;
    line1Widget.text = line1;
    line1Widget.flags = 0;
    line1Widget.x = style->paddingLeft + (textWidth - line1Width) / 2;
    line1Widget.y = yText;
    line1Widget.w = line1Width;
    line1Widget.h = textHeight;

    yText += textHeight;

    if (line2) {
        line2Widget.type = WIDGET_TYPE_TEXT;
        line2Widget.data = DATA_ID_NONE;
        line2Widget.action = ACTION_ID_NONE;
        line2Widget.style = styleId;
        line2Widget.text = line2;
        line2Widget.flags = 0;
        line2Widget.x = style->paddingLeft + (textWidth - line2Width) / 2;
        line2Widget.y = yText;
        line2Widget.w = line2Width;
        line2Widget.h = textHeight;

        yText += textHeight;

        if (line3) {
            line3Widget.type = WIDGET_TYPE_TEXT;
            line3Widget.data = DATA_ID_NONE;
            line3Widget.action = ACTION_ID_NONE;
            line3Widget.style = styleId;
            line3Widget.text = line3;
            line3Widget.flags = 0;
            line3Widget.x = style->paddingLeft + (textWidth - line3Width) / 2;
            line3Widget.y = yText;
            line3Widget.w = line3Width;
            line3Widget.h = textHeight;

            yText += textHeight;
        } else {
            line3Widget.type = WIDGET_TYPE_NONE;
        }
    } else {
        line2Widget.type = WIDGET_TYPE_NONE;
    }

    if (actionLabel) {
        actionWidget.type = WIDGET_TYPE_BUTTON;
        actionWidget.data = DATA_ID_NONE;
        actionWidget.style = STYLE_ID_ERROR_ALERT_BUTTON;
        actionWidget.text = actionLabel;
        actionWidget.x = style->paddingLeft + (textWidth - actionLabelWidth) / 2;
        actionWidget.y = style->paddingTop + yText;
        actionWidget.w = actionLabelWidth;
        actionWidget.h = actionLabelHeight;

		yText += textHeight;
    }

	x = appContext->rect.x + (appContext->rect.w - width) / 2;
	y = appContext->rect.y + (appContext->rect.h - height) / 2;
}

void ToastMessagePage::updateInternalPage() {
	WidgetCursor &widgetCursor = g_widgetCursor;

	if (widgetCursor.hasPreviousState && g_activeWidget == lastActiveWidget) {
		return;
	}

    lastActiveWidget = g_activeWidget;

    auto savedWidgetCursor = g_widgetCursor;
    g_widgetCursor = widgetCursor;

    widgetCursor.widget = &containerRectangleWidget;
    widgetCursor.x = x + containerRectangleWidget.x;
    widgetCursor.y = y + containerRectangleWidget.y;
    RectangleWidgetState rectangleWidgetState;
    rectangleWidgetState.flags.active = 0;
    rectangleWidgetState.render();

    if (g_findCallback == nullptr && !widgetCursor.refreshed) {
	    widgetCursor.pushBackground(widgetCursor.x, widgetCursor.y, getStyle(containerRectangleWidget.style), false);
    }

    TextWidgetState textWidgetState;
    textWidgetState.flags.active = 0;
	textWidgetState.flags.blinking = 0;
	textWidgetState.flags.focused = 0;

    widgetCursor.widget = &line1Widget;
    widgetCursor.x = x + line1Widget.x;
    widgetCursor.y = y + line1Widget.y;
    textWidgetState.render();

    if (line2Widget.type == WIDGET_TYPE_TEXT) {
        widgetCursor.widget = &line2Widget;
        widgetCursor.x = x + line2Widget.x;
        widgetCursor.y = y + line2Widget.y;
        textWidgetState.render();

        if (line3Widget.type == WIDGET_TYPE_TEXT) {
            widgetCursor.widget = &line3Widget;
            widgetCursor.x = x + line3Widget.x;
            widgetCursor.y = y + line3Widget.y;
            textWidgetState.render();
        }
    }

    if (actionWidget.type == WIDGET_TYPE_BUTTON) {
        ButtonWidgetState buttonWidgetState;

		buttonWidgetState.flags.active = g_activeWidget.widget == &actionWidget;
		buttonWidgetState.flags.blinking = 0;
		buttonWidgetState.flags.focused = 0;
		buttonWidgetState.flags.enabled = 1;
		widgetCursor.widget = &actionWidget;
        widgetCursor.x = x + actionWidget.x;
        widgetCursor.y = y + actionWidget.y;

        buttonWidgetState.render();
    }

    if (g_findCallback == nullptr && !widgetCursor.refreshed) {
	    widgetCursor.popBackground();
    }

	g_widgetCursor = savedWidgetCursor;
}

WidgetCursor ToastMessagePage::findWidgetInternalPage(int x, int y, bool clicked) {
    if (actionWidget.type == WIDGET_TYPE_BUTTON) {
        auto xActionWidget = this->x + actionWidget.x;
        auto yActionWidget = this->y + actionWidget.y;

        if (
            x >= xActionWidget && x < xActionWidget + actionWidget.w && 
            y >= yActionWidget && y < yActionWidget + actionWidget.h
        ) {
			WidgetCursor widgetCursor = g_widgetCursor;
			widgetCursor.appContext = appContext;
			widgetCursor.widget = &actionWidget;
            widgetCursor.x = xActionWidget;
            widgetCursor.y = yActionWidget;
            return widgetCursor;
        }
    }
   
    return WidgetCursor();
}

void ToastMessagePage::onEncoder(int counter) {
	toastMessagePageOnEncoderHook(this, counter);
}

void ToastMessagePage::onEncoderClicked() {
    if (hasAction()) {
		WidgetCursor widgetCursor;
        eez::gui::executeAction(widgetCursor, (int)(int16_t)actionWidget.action);
    } else {
		appContext->popPage();
    }
}

bool ToastMessagePage::canClickPassThrough() {
    return !hasAction();
}

bool ToastMessagePage::closeIfTouchedOutside() {
    return hasAction();
}

void ToastMessagePage::executeAction() {
    auto appContext = g_toastMessagePage.appContext;
    appContext->popPage();
    g_toastMessagePage.action(g_toastMessagePage.actionParam);
}

void ToastMessagePage::executeActionWithoutParam() {
    auto appContext = g_toastMessagePage.appContext;
    appContext->popPage();
    if (g_toastMessagePage.actionWithoutParam) {
        g_toastMessagePage.actionWithoutParam();
    }
}

////////////////////////////////////////////////////////////////////////////////

void SelectFromEnumPage::init(
    AppContext *appContext_,
    const EnumItem *enumDefinition_,
    uint16_t currentValue_,
    bool (*disabledCallback_)(uint16_t value),
    void (*onSet_)(uint16_t),
    bool smallFont_,
    bool showRadioButtonIcon_
) {
    appContext = appContext_;
	enumDefinition = enumDefinition_;
	enumDefinitionFunc = nullptr;
	currentValue = currentValue_;
	disabledCallback = disabledCallback_;
	onSet = onSet_;
    smallFont = smallFont_;
    showRadioButtonIcon = showRadioButtonIcon_;

	init();
}

void SelectFromEnumPage::init(
    AppContext *appContext_,
    void (*enumDefinitionFunc_)(DataOperationEnum operation, const WidgetCursor &widgetCursor, Value &value),
    uint16_t currentValue_,
    bool (*disabledCallback_)(uint16_t value),
    void (*onSet_)(uint16_t),
    bool smallFont_,
    bool showRadioButtonIcon_
) {
    appContext = appContext_;
	enumDefinition = nullptr;
	enumDefinitionFunc = enumDefinitionFunc_;
	currentValue = currentValue_;
	disabledCallback = disabledCallback_;
	onSet = onSet_;
    smallFont = smallFont_;
    showRadioButtonIcon = showRadioButtonIcon_;

    init();
}


void SelectFromEnumPage::init() {
    numColumns = 1;
    if (!calcSize()) {
        numColumns = 2;
        calcSize();
    }

    activeItemIndex = -1;

    findPagePosition();
}

uint16_t SelectFromEnumPage::getValue(int i) {
    if (enumDefinitionFunc) {
        Value value;
		WidgetCursor widgetCursor;
		widgetCursor.cursor = i;
        enumDefinitionFunc(DATA_OPERATION_GET_VALUE, widgetCursor, value);
        return value.getUInt8();
    }
    
    return enumDefinition[i].value;
}

bool SelectFromEnumPage::getLabel(int i, char *text, int count) {
    if (enumDefinitionFunc) {
        Value value;
		WidgetCursor widgetCursor;
		widgetCursor.cursor = i;
		enumDefinitionFunc(DATA_OPERATION_GET_LABEL, widgetCursor, value);
        if (value.getType() != VALUE_TYPE_UNDEFINED) {
            if (text) {
                value.toText(text, count);
            }
            return true;
        }
        return false;
    }

    if (enumDefinition[i].menuLabel) {
        if (text) {
            stringCopy(text, count, enumDefinition[i].menuLabel);
        }
        return true;
    }

    return false;
}

bool SelectFromEnumPage::isDisabled(int i) {
    return disabledCallback && disabledCallback(getValue(i));
}

bool SelectFromEnumPage::calcSize() {
    const Style *containerStyle = getStyle(smallFont ? STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER_S : STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER);
    const Style *itemStyle = getStyle(smallFont ? STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM_S : STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM);

    font::Font font = styleGetFont(itemStyle);

    // calculate geometry
    itemHeight = itemStyle->paddingTop + font.getHeight() + itemStyle->paddingBottom;
    itemWidth = 0;

    char text[64];

    numItems = 0;
    for (int i = 0; getLabel(i, nullptr, 0); ++i) {
        ++numItems;
    }

    for (int i = 0; i < numItems; ++i) {
        getItemLabel(i, text, sizeof(text));
        int width = display::measureStr(text, -1, font);
        if (width > itemWidth) {
            itemWidth = width;
        }
    }

    itemWidth = itemStyle->paddingLeft + itemWidth + itemStyle->paddingRight;

    width = containerStyle->paddingLeft + (numColumns == 2 ? itemWidth + containerStyle->paddingLeft + itemWidth : itemWidth) + containerStyle->paddingRight;
    if (width > appContext->rect.w) {
        width = appContext->rect.w;
    }

    height = containerStyle->paddingTop + (numColumns == 2 ? (numItems + 1) / 2 : numItems) * itemHeight + containerStyle->paddingBottom;
    if (height > appContext->rect.h) {
        if (numColumns == 1) {
            return false;
        }
        height = appContext->rect.h;
    }

    return true;
}

void SelectFromEnumPage::findPagePosition() {
    const WidgetCursor &widgetCursorAtTouchDown = getFoundWidgetAtDown();
    if (widgetCursorAtTouchDown.widget) {
        x = widgetCursorAtTouchDown.x;
        int xMargin = MAX(MIN(22, (appContext->rect.w - width) / 2), 0);
        int right = appContext->rect.x + appContext->rect.w - xMargin;
        if (x + width > right) {
            x = right - width;
        }

        y = widgetCursorAtTouchDown.y + widgetCursorAtTouchDown.widget->h;
        int yMargin = MAX(MIN(30, (appContext->rect.h - height) / 2), 0);
        int bottom = appContext->rect.y + appContext->rect.h - yMargin;
        if (y + height > bottom) {
            y = bottom - height;
        }
    } else {
        x = appContext->rect.x + (appContext->rect.w - width) / 2;
        y = appContext->rect.y + (appContext->rect.h - height) / 2;
    }
}

void SelectFromEnumPage::updateInternalPage() {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    if (widgetCursor.hasPreviousState && !dirty) {
		return;
    }

    const Style *containerStyle = getStyle(smallFont ? STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER_S : STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER);
	const Style *itemStyle = getStyle(smallFont ? STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM_S : STYLE_ID_SELECT_ENUM_ITEM_POPUP_ITEM);
	const Style *disabledItemStyle = getStyle(smallFont ? STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM_S : STYLE_ID_SELECT_ENUM_ITEM_POPUP_DISABLED_ITEM);

    // draw background
    display::setColor(containerStyle->backgroundColor);
    display::fillRect(x, y, x + width - 1, y + height - 1);

    // draw labels
    char text[64];
    for (int i = 0; getLabel(i, nullptr, 0); ++i) {
        int xItem, yItem;
        getItemPosition(i, xItem, yItem);

        getItemLabel(i, text, sizeof(text));
        drawText(
            text, -1,
            xItem, yItem, itemWidth, itemHeight,
            isDisabled(i) ? disabledItemStyle : itemStyle,
            i == activeItemIndex
        );
    }

    dirty = false;
}

WidgetCursor SelectFromEnumPage::findWidgetInternalPage(int x, int y, bool clicked) {
    const WidgetCursor &widgetCursor = g_widgetCursor;

    for (int i = 0; i < numItems; ++i) {
        int xItem, yItem;
        getItemPosition(i, xItem, yItem);
        if (!isDisabled(i)) {
        	if (x >= xItem && x < xItem + itemWidth && y >= yItem && y < yItem + itemHeight) {
                
                if (clicked) {
                    activeItemIndex = i;
                    currentValue = getValue(i);
                    dirty = true;
                }

        		widget.action = ACTION_ID_INTERNAL_SELECT_ENUM_ITEM;
        		widget.data = (uint16_t)i;
        		return WidgetCursor(g_mainAssets, appContext, &widget, x, y, widgetCursor.currentState, widgetCursor.refreshed, widgetCursor.hasPreviousState);
        	}
        }
    }

    return WidgetCursor();
}

void SelectFromEnumPage::selectEnumItem() {
    int itemIndex = getFoundWidgetAtDown().widget->data;
	if (onSet) {
		onSet(getValue(itemIndex));
	} else {
		appContext->popPage();
	}
}

void SelectFromEnumPage::getItemPosition(int itemIndex, int &xItem, int &yItem) {
    const Style *containerStyle = getStyle(smallFont ? STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER_S : STYLE_ID_SELECT_ENUM_ITEM_POPUP_CONTAINER);

    if (numColumns == 1 || itemIndex < (numItems + 1) / 2) {
        xItem = x + containerStyle->paddingLeft;
        yItem = y + containerStyle->paddingTop + itemIndex * itemHeight;
    } else {
        xItem = x + containerStyle->paddingLeft + itemWidth + containerStyle->paddingLeft / 2;
        yItem = y + containerStyle->paddingTop + (itemIndex - (numItems + 1) / 2) * itemHeight;
    }
}

void SelectFromEnumPage::getItemLabel(int itemIndex, char *text, int count) {
    if (showRadioButtonIcon) {
        if (getValue(itemIndex) == currentValue) {
            text[0] = (char)142;
        } else {
            text[0] = (char)141;
        }

        text[1] = ' ';

        getLabel(itemIndex, text + 2, count - 2);
    } else {
        getLabel(itemIndex, text, count);
    }
}

////////////////////////////////////////////////////////////////////////////////

void showMenu(AppContext *appContext, const char *message, MenuType menuType, const char **menuItems, void(*callback)(int)) {
    if (menuType == MENU_TYPE_BUTTON) {
		appContext->pushPage(INTERNAL_PAGE_ID_MENU_WITH_BUTTONS, MenuWithButtonsPage::create(appContext, message, menuItems, callback));
    }
}

////////////////////////////////////////////////////////////////////////////////

static MenuWithButtonsPage g_menuWithButtonsPage;

MenuWithButtonsPage *MenuWithButtonsPage::create(AppContext *appContext, const char *message, const char **menuItems, void(*callback)(int)) {
    MenuWithButtonsPage *page = &g_menuWithButtonsPage;

    page->init(appContext, message, menuItems, callback);

    return page;
}

void MenuWithButtonsPage::init(AppContext *appContext, const char *message, const char **menuItems, void(*callback)(int)) {
    m_appContext = appContext;
    m_callback = callback;

    m_containerRectangleWidget.type = WIDGET_TYPE_RECTANGLE;
    m_containerRectangleWidget.data = DATA_ID_NONE;
    m_containerRectangleWidget.action = ACTION_ID_NONE;
    m_containerRectangleWidget.style = STYLE_ID_MENU_WITH_BUTTONS_CONTAINER;
    m_containerRectangleWidget.flags.ignoreLuminosity = 0;

    m_messageTextWidget.type = WIDGET_TYPE_TEXT;
    m_messageTextWidget.data = DATA_ID_NONE;
    m_messageTextWidget.action = ACTION_ID_NONE;
    m_messageTextWidget.style = STYLE_ID_MENU_WITH_BUTTONS_MESSAGE;
    m_messageTextWidget.text = message;
    m_messageTextWidget.flags = 0;
    TextWidget_autoSize(g_mainAssets, m_messageTextWidget);

    size_t i;

    for (i = 0; menuItems[i]; i++) {
        m_buttonTextWidgets[i].type = WIDGET_TYPE_TEXT;
        m_buttonTextWidgets[i].data = DATA_ID_NONE;
        m_buttonTextWidgets[i].action = ACTION_ID_INTERNAL_MENU_WITH_BUTTONS;
        m_buttonTextWidgets[i].style = STYLE_ID_MENU_WITH_BUTTONS_BUTTON;
        m_buttonTextWidgets[i].text = menuItems[i];
        m_buttonTextWidgets[i].flags = 0;
        TextWidget_autoSize(g_mainAssets, m_buttonTextWidgets[i]);
    }

    m_numButtonTextWidgets = i;

    const Style *styleContainer = getStyle(STYLE_ID_MENU_WITH_BUTTONS_CONTAINER);
    const Style *styleButton = getStyle(STYLE_ID_MENU_WITH_BUTTONS_BUTTON);

    int maxMenuItemWidth = 0;
    for (size_t i = 0; i < m_numButtonTextWidgets; i++) {
        maxMenuItemWidth = MAX(maxMenuItemWidth, m_buttonTextWidgets[i].w);
    }

    int menuItemsWidth = maxMenuItemWidth * m_numButtonTextWidgets + (m_numButtonTextWidgets - 1) * styleButton->paddingLeft;

    int contentWidth = MAX(m_messageTextWidget.w, menuItemsWidth);
    int contentHeight = m_messageTextWidget.h + m_buttonTextWidgets[0].h;

    width = styleContainer->borderSizeLeft + styleContainer->paddingLeft + contentWidth + styleContainer->paddingRight + styleContainer->borderSizeRight;
    height = styleContainer->borderSizeTop + styleContainer->paddingTop + contentHeight + styleContainer->paddingBottom + styleContainer->borderSizeBottom;

    x = m_appContext->rect.x + (m_appContext->rect.w - width) / 2;
    y = m_appContext->rect.y + (m_appContext->rect.h - height) / 2;

    m_containerRectangleWidget.x = 0;
    m_containerRectangleWidget.y = 0;
    m_containerRectangleWidget.w = width;
    m_containerRectangleWidget.h = height;

    m_messageTextWidget.x = styleContainer->borderSizeLeft + styleContainer->paddingLeft + (contentWidth - m_messageTextWidget.w) / 2;
    m_messageTextWidget.y = styleContainer->borderSizeTop + styleContainer->paddingTop;

    int xButtonTextWidget = styleContainer->borderSizeLeft + styleContainer->paddingLeft + (contentWidth - menuItemsWidth) / 2;
    int yButtonTextWidget = styleContainer->borderSizeTop + styleContainer->paddingTop + m_messageTextWidget.h;
    for (size_t i = 0; i < m_numButtonTextWidgets; i++) {
        m_buttonTextWidgets[i].x = xButtonTextWidget;
        m_buttonTextWidgets[i].y = yButtonTextWidget;
        m_buttonTextWidgets[i].w = maxMenuItemWidth;
        xButtonTextWidget += maxMenuItemWidth + styleButton->paddingLeft;
    }

}

void MenuWithButtonsPage::updateInternalPage() {
    WidgetCursor widgetCursor = g_widgetCursor;

    auto savedWidgetCursor = g_widgetCursor;
    g_widgetCursor = widgetCursor;

    widgetCursor.widget = &m_containerRectangleWidget;
    widgetCursor.x = x + m_containerRectangleWidget.x;
    widgetCursor.y = y + m_containerRectangleWidget.y;
    RectangleWidgetState rectangleWidgetState;
    rectangleWidgetState.flags.active = 0;
    rectangleWidgetState.render();

    if (g_findCallback == nullptr && !widgetCursor.refreshed) {
	    widgetCursor.pushBackground(widgetCursor.x, widgetCursor.y, getStyle(m_containerRectangleWidget.style), false);
    }

    widgetCursor.widget = &m_messageTextWidget;
    widgetCursor.x = x + m_messageTextWidget.x;
    widgetCursor.y = y + m_messageTextWidget.y;
    TextWidgetState textWidgetState;
    textWidgetState.flags.active = 0;
	textWidgetState.flags.blinking = 0;
	textWidgetState.flags.focused = 0;
    textWidgetState.render();

    for (size_t i = 0; i < m_numButtonTextWidgets; i++) {
        widgetCursor.widget = &m_buttonTextWidgets[i];
        widgetCursor.x = x + m_buttonTextWidgets[i].x;
        widgetCursor.y = y + m_buttonTextWidgets[i].y;
        widgetCursor.cursor = i;
		TextWidgetState textWidgetState;
		textWidgetState.flags.active = widgetCursor == g_activeWidget;
		textWidgetState.flags.blinking = 0;
		textWidgetState.flags.focused = 0;
		textWidgetState.render();
    }

    if (g_findCallback == nullptr && !widgetCursor.refreshed) {
	    widgetCursor.popBackground();
    }

    g_widgetCursor = savedWidgetCursor;
}

WidgetCursor MenuWithButtonsPage::findWidgetInternalPage(int x, int y, bool clicked) {
    WidgetCursor widgetCursor = g_widgetCursor;
    widgetCursor.appContext = m_appContext;

    for (size_t i = 0; i < m_numButtonTextWidgets; i++) {
        widgetCursor.widget = &m_buttonTextWidgets[i];
        widgetCursor.x = this->x + m_buttonTextWidgets[i].x;
        widgetCursor.y = this->y + m_buttonTextWidgets[i].y;
        widgetCursor.cursor = i;
        if (
            x >= widgetCursor.x && x < widgetCursor.x + m_buttonTextWidgets[i].w && 
            y >= widgetCursor.y && y < widgetCursor.y + m_buttonTextWidgets[i].h
        ) {
            return widgetCursor;
        }
    }

    widgetCursor.widget = &m_containerRectangleWidget;
    widgetCursor.x = this->x + m_containerRectangleWidget.x;
    widgetCursor.y = this->y + m_containerRectangleWidget.y;

    return widgetCursor;
}

void MenuWithButtonsPage::executeAction() {
    (*g_menuWithButtonsPage.m_callback)(getFoundWidgetAtDown().cursor);
}

} // namespace gui
} // namespace eez
