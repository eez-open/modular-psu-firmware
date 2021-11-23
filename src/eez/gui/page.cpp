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

} // namespace gui
} // namespace eez
