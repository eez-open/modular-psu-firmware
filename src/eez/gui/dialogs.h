/*
 * EEZ Generic Firmware
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

#include <stdlib.h>

#include <eez/gui/data.h>

namespace eez {
namespace gui {

extern data::Value g_alertMessage;
extern data::Value g_alertMessage2;
extern data::Value g_alertMessage3;
extern data::Value g_progress;
extern char g_throbber[8];

void showAsyncOperationInProgress(const char *message, void (*checkStatus)() = 0);
void hideAsyncOperationInProgress();

void showProgressPage(const char *message, void (*abortCallback)() = 0);
bool updateProgressPage(size_t processedSoFar, size_t totalSize);
void hideProgressPage();

void setTextMessage(const char *message, unsigned int len);
void clearTextMessage();
const char *getTextMessage();
uint8_t getTextMessageVersion();

void infoMessage(const char *message, void (*ok_callback)() = 0);
void infoMessage(const char *message1, const char *message2, void(*ok_callback)() = 0);
void toastMessage(const char *message1, const char *message2, const char *message3, void (*callback)() = 0);
void errorMessage(const char *message, void (*callback)() = 0);
void errorMessage(const char *message1, const char *message2, void (*callback)() = 0);
void errorMessage(data::Value value, void (*callback)() = 0);

void errorMessageWithAction(data::Value value, void (*ok_callback)(), void (*action)(int param), const char *actionLabel, int actionParam);
void errorMessageAction();

void yesNoDialog(int yesNoPageId, const char *message, void (*yes_callback)(),
                 void (*no_callback)(), void (*cancel_callback)());
void yesNoLater(const char *message, void (*yes_callback)(), void (*no_callback)(),
                void (*later_callback)() = 0);
void areYouSure(void (*yes_callback)());
void areYouSureWithMessage(const char *message, void (*yes_callback)());

void dialogYes();
void dialogNo();
void dialogCancel();
void dialogOk();
void dialogLater();

} // namespace gui
} // namespace eez
