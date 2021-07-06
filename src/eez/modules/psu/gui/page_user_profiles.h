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

#include <eez/modules/psu/profile.h>
#include <eez/gui/gui.h>
using namespace eez::gui;

namespace eez {
namespace psu {
namespace gui {

class UserProfilesPage : public Page {
public:
    static int getSelectedProfileLocation();
    
    void showProfile();

    static void toggleAutoRecall();
    void toggleIsAutoRecallLocation();

    void saveProfile();
    static void doSaveProfile();

    void recallProfile();
    static void doRecallProfile();

    void importProfile();
    static void doImportProfile();

    void exportProfile();
    static void doExportProfile();

    void deleteProfile();
    static void doDeleteProfile();
    
    void editRemark();
    static void doEditRemark();

    void onAsyncOperationFinished(int16_t err);

private:
    char m_profileFilePath[MAX_PATH_LENGTH + 1];

    static void onSaveFinish(char *remark = 0, void (*callback)() = 0);
    static void onSaveEditRemarkOk(char *remark);
    static void onSaveYes();
    static void onImportProfileFileSelected(const char *listFilePath);
    static void onExportProfileFileSelected(const char *listFilePath);
    static void onDeleteProfileYes();
    static void onEditRemarkOk(char *newRemark);
};

} // namespace gui
} // namespace psu
} // namespace eez
