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
    virtual void set();
    virtual void discard();

    virtual bool showAreYouSureOnDiscard();
};

class SetPage : public Page {
public:
    virtual void edit();

protected:
    int16_t editDataId;

    static void onSetValue(float value);
    virtual void setValue(float value);
};

class InternalPage : public Page {
public:
    virtual void updateInternalPage(const WidgetCursor &widgetCursor, WidgetState *currentState, WidgetState *previousState) = 0;
	virtual WidgetCursor findWidgetInternalPage(int x, int y, bool clicked) = 0;
    virtual bool canClickPassThrough();

    int x;
    int y;
    int width;
    int height;

protected:
    Widget widget;
};

} // namespace gui
} // namespace eez
