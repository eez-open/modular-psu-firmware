/*
 * EEZ Modular Firmware
 * Copyright (C) 2020-present, Envox d.o.o.
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

void action_yes() {
    auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    auto callback = appContext->m_dialogYesCallback;
    appContext->popPage();
    if (callback) {
        callback();
    }
}

void action_no() {
    auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    auto callback = appContext->m_dialogNoCallback;
    appContext->popPage();
    if (callback) {
        callback();
    }
}

void action_ok() {
	action_yes();
}

void action_cancel() {
    auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    auto callback = appContext->m_dialogCancelCallback;
    appContext->popPage();
    if (callback) {
        callback();
    }
}

void action_later() {
    auto appContext = getAppContextFromId(APP_CONTEXT_ID_DEVICE);
    auto callback = appContext->m_dialogLaterCallback;
    appContext->popPage();
    if (callback) {
        callback();
    }
}

void action_drag_overlay() {
}

void action_scroll() {
}

} // namespace gui
} // namespace eez