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

#include <eez/gui/widgets/text.h>
#include <eez/gui/widgets/rectangle.h>

namespace eez {
namespace gui {

class Page {
  public:
    virtual ~Page() {
    }

    virtual void pageAlloc();
    virtual void pageFree();

    virtual void pageWillAppear();

    virtual void onEncoder(int counter);
    virtual void onEncoderClicked();
    virtual Unit getEncoderUnit();

    virtual int getDirty();
    virtual bool showAreYouSureOnDiscard();
};

class SetPage : public Page {
  public:
    virtual void edit();
    virtual void set() = 0;
    virtual void discard();

  protected:
    int16_t editDataId;

    static void onSetValue(float value);
    virtual void setValue(float value);
};

class InternalPage : public Page {
  public:
    virtual void refresh(const WidgetCursor& widgetCursor) = 0; // repaint page
    virtual void updatePage(const WidgetCursor& widgetCursor) = 0;
	virtual WidgetCursor findWidget(int x, int y) = 0;

    int x;
    int y;
    int width;
    int height;

  protected:
    Widget widget;
};

////////////////////////////////////////////////////////////////////////////////

enum ToastType {
    INFO_TOAST,
    ERROR_TOAST
};

class ToastMessagePage : public InternalPage {
    static ToastMessagePage *findFreePage();

public:
    static ToastMessagePage *create(ToastType type, const char *message1);
    static ToastMessagePage *create(ToastType type, data::Value message1Value);
    static ToastMessagePage *create(ToastType type, const char *message1, const char *message2);
    static ToastMessagePage *create(ToastType type, const char *message1, const char *message2, const char *message3, bool autoDismiss = false);
    static ToastMessagePage *create(ToastType type, data::Value message1Value, void (*action)(int param), const char *actionLabel, int actionParam);
    static ToastMessagePage *create(ToastType type, const char *message, void (*action)(), const char *actionLabel);

    void pageFree();

    void onEncoder(int counter);
    void onEncoderClicked();

    void refresh(const WidgetCursor& widgetCursor);
    void updatePage(const WidgetCursor& widgetCursor);
    WidgetCursor findWidget(int x, int y);

    bool hasAction() {
      return actionWidget.action != 0;
    }

    static void executeAction();
    static void executeActionWithoutParam();

    AppContext *getAppContext() { return appContext; }

private:
    ToastType type;

    const char *message1;
    data::Value message1Value;
    const char *message2;
    const char *message3;
    const char *actionLabel;

    Widget actionWidget;
    bool actionWidgetIsActive;

    AppContext *appContext;
};

////////////////////////////////////////////////////////////////////////////////

class SelectFromEnumPage : public InternalPage {
  public:
    void init(const data::EnumItem *enumDefinition_, uint16_t currentValue_,
    		bool (*disabledCallback_)(uint16_t value), void (*onSet_)(uint16_t), bool smallFont_, bool showRadioButtonIcon_);
    void init(void (*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value),
        uint16_t currentValue_, bool (*disabledCallback_)(uint16_t value), void (*onSet_)(uint16_t), bool smallFont_, bool showRadioButtonIcon_);

    void init();

    void refresh(const WidgetCursor& widgetCursor);
    void updatePage(const WidgetCursor& widgetCursor);
    WidgetCursor findWidget(int x, int y);

    void selectEnumItem();

    const data::EnumItem *getEnumDefinition() {
        return enumDefinition;
    }

  private:
    const data::EnumItem *enumDefinition;
    void (*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value);

    int numItems;
    int numColumns;
    int itemWidth;
    int itemHeight;

    int activeItemIndex;

    bool dirty;

    uint16_t getValue(int i);
    bool getLabel(int i, char *text, int count);

    uint16_t currentValue;
    bool (*disabledCallback)(uint16_t value);
    void (*onSet)(uint16_t);

    bool smallFont;
    bool showRadioButtonIcon;

    bool calcSize();
    void findPagePosition();

    bool isDisabled(int i);

    void getItemPosition(int itemIndex, int &x, int &y);
    void getItemLabel(int itemIndex, char *text, int count);
};

////////////////////////////////////////////////////////////////////////////////

enum MenuType {
    MENU_TYPE_BUTTON
};

static const size_t MAX_MENU_ITEMS = 4;

void showMenu(AppContext *appContext, const char *message, MenuType menuType, const char **menuItems, void(*callback)(int));

class MenuWithButtonsPage : public InternalPage {
public:
    static MenuWithButtonsPage *create(AppContext *appContext, const char *message, const char **menuItems, void (*callback)(int));

    void refresh(const WidgetCursor& widgetCursor);
    void updatePage(const WidgetCursor& widgetCursor);
    WidgetCursor findWidget(int x, int y);

    static void executeAction();

private:
    AppContext *m_appContext;
    RectangleWidget m_containerRectangleWidget;
    TextWidget m_messageTextWidget;
    TextWidget m_buttonTextWidgets[MAX_MENU_ITEMS];
    size_t m_numButtonTextWidgets;
    void (*m_callback)(int);

    void init(AppContext *appContext, const char *message, const char **menuItems, void(*callback)(int));
};

////////////////////////////////////////////////////////////////////////////////

class DialogPage : public InternalPage {
public:

private:
};

} // namespace gui
} // namespace eez
