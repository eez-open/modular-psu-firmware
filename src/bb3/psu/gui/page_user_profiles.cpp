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

#include <bb3/psu/psu.h>
#include <bb3/psu/channel_dispatcher.h>

#include <bb3/psu/gui/psu.h>
#include <bb3/psu/gui/keypad.h>
#include <bb3/psu/gui/page_user_profiles.h>
#include <bb3/psu/gui/file_manager.h>

#include <scpi/scpi.h>

using namespace eez::psu::gui;

namespace eez {
namespace psu {
namespace gui {

static int g_selectedProfileLocation = -1;
char g_remark[PROFILE_NAME_MAX_LENGTH + 1];

UserProfilesPage *getUserProfileSettingsPage() {
    Page *page = getPage(PAGE_ID_USER_PROFILE_SETTINGS);
    if (!page) {
        page = getPage(PAGE_ID_USER_PROFILE_0_SETTINGS);
    }
    return (UserProfilesPage *)page;
}


int UserProfilesPage::getSelectedProfileLocation() {
    if (getActivePageId() == PAGE_ID_USER_PROFILE_0_SETTINGS || getActivePageId() == PAGE_ID_USER_PROFILE_SETTINGS) {
        return g_selectedProfileLocation;
    }
    return -1;
}

void UserProfilesPage::showProfile() {
    g_selectedProfileLocation = getFoundWidgetAtDown().cursor;
    pushPage(g_selectedProfileLocation == 0 ? PAGE_ID_USER_PROFILE_0_SETTINGS : PAGE_ID_USER_PROFILE_SETTINGS);
}

////////////////////////////////////////////////////////////////////////////////

void UserProfilesPage::toggleAutoRecall() {
    bool enable = persist_conf::isProfileAutoRecallEnabled() ? false : true;
    persist_conf::enableProfileAutoRecall(enable);
}

void UserProfilesPage::toggleIsAutoRecallLocation() {
    if (g_selectedProfileLocation == 0 || profile::isValid(g_selectedProfileLocation)) {
        persist_conf::setProfileAutoRecallLocation(g_selectedProfileLocation);
    }
}

////////////////////////////////////////////////////////////////////////////////

void UserProfilesPage::saveProfile() {
    if (g_selectedProfileLocation > 0) {
        if (profile::isValid(g_selectedProfileLocation)) {
            areYouSure(onSaveYes);
        } else {
            onSaveYes();
        }
    }
}

void UserProfilesPage::onSaveYes() {
    if (g_selectedProfileLocation > 0) {
        char remark[PROFILE_NAME_MAX_LENGTH + 1];
        profile::getSaveName(g_selectedProfileLocation, remark, sizeof(remark));

        Keypad::startPush(0, remark, 0, PROFILE_NAME_MAX_LENGTH, false, onSaveEditRemarkOk, 0);
    } else {
        onSaveFinish();
    }
}

void UserProfilesPage::onSaveEditRemarkOk(char *remark) {
    onSaveFinish(remark, popPage);
}

void UserProfilesPage::onSaveFinish(char *remark, void (*callback)()) {
    if (callback) {
        callback();
    }

    stringCopy(g_remark, sizeof(g_remark), remark);

    showProgressPageWithoutAbort("Saving profile...");

    sendMessageToLowPriorityThread(THREAD_MESSAGE_USER_PROFILES_PAGE_SAVE);
}

void UserProfilesPage::doSaveProfile() {
    int err;
    profile::saveToLocation(g_selectedProfileLocation, g_remark, true, &err);

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED, err);
}

////////////////////////////////////////////////////////////////////////////////

void UserProfilesPage::recallProfile() {
    if (g_selectedProfileLocation > 0 && profile::isValid(g_selectedProfileLocation)) {
        showProgressPageWithoutAbort("Recalling profile...");

        sendMessageToLowPriorityThread(THREAD_MESSAGE_USER_PROFILES_PAGE_RECALL);
    }
}

void UserProfilesPage::doRecallProfile() {
    int err;
    profile::recallFromLocation(g_selectedProfileLocation, 0, true, &err);

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED, err);
}

////////////////////////////////////////////////////////////////////////////////

void UserProfilesPage::importProfile() {
    if (g_selectedProfileLocation > 0) {
        file_manager::browseForFile("Import profile", "/Profiles", FILE_TYPE_PROFILE, file_manager::DIALOG_TYPE_OPEN, onImportProfileFileSelected);
    }
}

void UserProfilesPage::onImportProfileFileSelected(const char *profileFilePath) {
    auto *page = (UserProfilesPage *)getUserProfileSettingsPage();
    stringCopy(page->m_profileFilePath, sizeof(page->m_profileFilePath), profileFilePath);
    
    showProgressPageWithoutAbort("Importing profile...");

    sendMessageToLowPriorityThread(THREAD_MESSAGE_USER_PROFILES_PAGE_IMPORT);
}

void UserProfilesPage::doImportProfile() {
    auto *page = (UserProfilesPage *)getUserProfileSettingsPage();

    int err;
    profile::importFileToLocation(page->m_profileFilePath, g_selectedProfileLocation, true, &err);

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED, err);
}

////////////////////////////////////////////////////////////////////////////////

void UserProfilesPage::exportProfile() {
    if (profile::isValid(g_selectedProfileLocation)) {
        file_manager::browseForFile("Export profile as", "/Profiles", FILE_TYPE_PROFILE, file_manager::DIALOG_TYPE_SAVE, onExportProfileFileSelected);
    }
}

void UserProfilesPage::onExportProfileFileSelected(const char *profileFilePath) {
    auto *page = (UserProfilesPage *)getUserProfileSettingsPage();
    stringCopy(page->m_profileFilePath, sizeof(page->m_profileFilePath), profileFilePath);
    
    showProgressPageWithoutAbort("Exporting profile...");

    sendMessageToLowPriorityThread(THREAD_MESSAGE_USER_PROFILES_PAGE_EXPORT);
}

void UserProfilesPage::doExportProfile() {
    auto *page = (UserProfilesPage *)getUserProfileSettingsPage();

    int err;
    profile::exportLocationToFile(g_selectedProfileLocation, page->m_profileFilePath, true, &err);

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED, err);
}

////////////////////////////////////////////////////////////////////////////////

void UserProfilesPage::deleteProfile() {
    if (g_selectedProfileLocation > 0 && profile::isValid(g_selectedProfileLocation)) {
        areYouSure(onDeleteProfileYes);
    }
}

void UserProfilesPage::onDeleteProfileYes() {
    showProgressPageWithoutAbort("Deleting profile...");

    sendMessageToLowPriorityThread(THREAD_MESSAGE_USER_PROFILES_PAGE_DELETE);
}

void UserProfilesPage::doDeleteProfile() {
    int err;
    profile::deleteLocation(g_selectedProfileLocation, true, &err);

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED, err);
}

////////////////////////////////////////////////////////////////////////////////

void UserProfilesPage::editRemark() {
    if (g_selectedProfileLocation > 0 && profile::isValid(g_selectedProfileLocation)) {
        char remark[PROFILE_NAME_MAX_LENGTH + 1];
        profile::getName(g_selectedProfileLocation, remark, sizeof(remark));
        Keypad::startPush(0, remark, 0, PROFILE_NAME_MAX_LENGTH, false, onEditRemarkOk, 0);
    }
}

void UserProfilesPage::onEditRemarkOk(char *newRemark) {
    stringCopy(g_remark, sizeof(g_remark), newRemark);

    popPage();

    showProgressPageWithoutAbort("Saving profile remark...");

    sendMessageToLowPriorityThread(THREAD_MESSAGE_USER_PROFILES_PAGE_EDIT_REMARK);
}

void UserProfilesPage::doEditRemark() {
    int err;
    profile::setName(g_selectedProfileLocation, g_remark, true, &err);

    sendMessageToGuiThread(GUI_QUEUE_MESSAGE_TYPE_USER_PROFILES_PAGE_ASYNC_OPERATION_FINISHED, err);
}

////////////////////////////////////////////////////////////////////////////////

void UserProfilesPage::onAsyncOperationFinished(int16_t err) {
    hideProgressPage();
    if (err == SCPI_RES_OK) {
        // infoMessage("Done!");
    } else {
        errorMessage(Value(err, VALUE_TYPE_SCPI_ERROR));
    }
}

} // namespace gui
} // namespace psu
} // namespace eez

#endif
