/*
 * EEZ Generic Firmware
 * Copyright (C) 2016-present, Envox d.o.o.
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

#include <eez/gui/widget.h>

namespace eez {
namespace gui {

class Page {
  public:
    virtual ~Page() {
    }

    virtual void pageWillAppear();

    virtual bool onEncoder(int counter);
    virtual bool onEncoderClicked();

    virtual int getDirty();
};

class SetPage : public Page {
  public:
    virtual void edit();
    virtual void set() = 0;
    virtual void discard();

  protected:
    int editDataId;

    static void onSetValue(float value);
    virtual void setValue(float value);
};

class InternalPage : public Page {
  public:
    virtual void refresh() = 0; // repaint page
    virtual bool updatePage() = 0; // repaint page if changed, returns true if painted
	  virtual WidgetCursor findWidget(int x, int y) = 0;

    int x;
    int y;
    int width;
    int height;

  protected:
	Widget widget;
};

enum AlertMessageType {
    INFO_ALERT,
    TOAST_ALERT,
    ERROR_ALERT
};

class AlertMessagePage : public InternalPage {
public:
    AlertMessagePage(AlertMessageType type, const char *message1, void (*callback)());
    AlertMessagePage(AlertMessageType type, data::Value message1Value, void(*callback)());
    AlertMessagePage(AlertMessageType type, const char *message1, const char *message2, void(*callback)());
    AlertMessagePage(AlertMessageType type, const char *message1, const char *message2, const char *message3, void(*callback)());

    void refresh();
    bool updatePage();
    WidgetCursor findWidget(int x, int y);

private:
    AlertMessageType type;

    const char *message1;
    data::Value message1Value;
    const char *message2;
    const char *message3;
};

class SelectFromEnumPage : public InternalPage {
  public:
    SelectFromEnumPage(const data::EnumItem *enumDefinition_, uint8_t currentValue_,
                       bool (*disabledCallback_)(uint8_t value), void (*onSet_)(uint8_t));
    SelectFromEnumPage(void (*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value), 
                       uint8_t currentValue_, bool (*disabledCallback_)(uint8_t value), void (*onSet_)(uint8_t));

    void refresh();
    bool updatePage();
    WidgetCursor findWidget(int x, int y);

    void selectEnumItem();

  private:
    const data::EnumItem *enumDefinition;
    void (*enumDefinitionFunc)(data::DataOperationEnum operation, data::Cursor &cursor, data::Value &value);

    uint8_t getValue(int i);
    const char *getLabel(int i);

    uint8_t currentValue;
    bool (*disabledCallback)(uint8_t value);
    void (*onSet)(uint8_t);

	void findPagePosition();

    const WidgetCursor &widgetCursorAtTouchDown;

    int numItems;
    int itemWidth;
    int itemHeight;

    bool isDisabled(int i);

    void getItemPosition(int itemIndex, int &x, int &y);
    void getItemLabel(int itemIndex, char *text, int count);
};

} // namespace gui
} // namespace eez
