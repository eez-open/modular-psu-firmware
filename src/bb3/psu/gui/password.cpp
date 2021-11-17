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

#include <bb3/psu/psu.h>
#include <bb3/psu/persist_conf.h>

#include <bb3/psu/gui/psu.h>
#include <bb3/psu/gui/keypad.h>
#include <bb3/psu/gui/password.h>

#include <scpi/scpi.h>

namespace eez {
namespace psu {
namespace gui {

static void (*g_checkPasswordOkCallback)();
static const char *g_oldPassword;
static char g_newPassword[PASSWORD_MAX_LENGTH];

////////////////////////////////////////////////////////////////////////////////

static void checkPasswordOkCallback(char *text) {
    int nPassword = strlen(g_oldPassword);
    int nText = strlen(text);
    if (nPassword == nText && strncmp(g_oldPassword, text, nText) == 0) {
        g_checkPasswordOkCallback();
    } else {
        // entered password doesn't match,
        popPage();
        errorMessage("Invalid password!");
    }
}

static void checkPassword(const char *label, void (*ok)()) {
    g_checkPasswordOkCallback = ok;
    Keypad::startPush(label, 0, 0, PASSWORD_MAX_LENGTH, true, checkPasswordOkCallback, popPage);
}

////////////////////////////////////////////////////////////////////////////////

static void onRetypeNewPasswordOk(char *text) {
    size_t textLen = strlen(text);
    if (strlen(g_newPassword) != textLen || strncmp(g_newPassword, text, textLen) != 0) {
        // retyped new password doesn't match
        popPage();
        errorMessage("Password doesn't match!");
        return;
    }

    if (g_oldPassword == persist_conf::devConf.systemPassword) {
        persist_conf::changeSystemPassword(g_newPassword, strlen(g_newPassword));
    } else {
        persist_conf::changeCalibrationPassword(g_newPassword, strlen(g_newPassword));
    }

    popPage();
}

static void onNewPasswordOk(char *text) {
    int textLength = strlen(text);

    bool isOk;
    int16_t err;
    if (g_oldPassword == persist_conf::devConf.systemPassword) {
        isOk = persist_conf::isSystemPasswordValid(text, textLength, err);
    } else {
        isOk = persist_conf::isCalibrationPasswordValid(text, textLength, err);
    }

    if (!isOk) {
        // invalid password, return to keypad
        if (err == SCPI_ERROR_PASSWORD_TOO_SHORT || err == SCPI_ERROR_PASSWORD_TOO_SHORT) {
            errorMessage("Password too short!");
        } else {
            errorMessage("Password too long!");
        }
        return;
    }

    stringCopy(g_newPassword, sizeof(g_newPassword), text);
    Keypad::startReplace("Retype new password: ", 0, 0, PASSWORD_MAX_LENGTH, true, onRetypeNewPasswordOk, popPage);
}

static void onOldPasswordOk() {
    Keypad::startReplace("New password: ", 0, 0, PASSWORD_MAX_LENGTH, true, onNewPasswordOk, popPage);
}

static void editPassword(const char *oldPassword) {
    g_oldPassword = oldPassword;

    if (strlen(g_oldPassword)) {
        checkPassword("Current password: ", onOldPasswordOk);
    } else {
        Keypad::startPush("New password: ", 0, 0, PASSWORD_MAX_LENGTH, true, onNewPasswordOk, popPage);
    }
}

////////////////////////////////////////////////////////////////////////////////

void checkPassword(const char *label, const char *password, void (*ok)()) {
    if (strlen(password) == 0) {
        ok();
    } else {
        g_oldPassword = password;
        checkPassword(label, ok);
    }
}

void editSystemPassword() {
    editPassword(persist_conf::devConf.systemPassword);
}

void editCalibrationPassword() {
    editPassword(persist_conf::devConf.calibrationPassword);
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif