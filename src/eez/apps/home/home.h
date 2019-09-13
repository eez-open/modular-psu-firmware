/*
 * EEZ Home Application
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

#include <eez/gui/app_context.h>

using namespace eez::gui;

namespace eez {
namespace home {

class HomeAppContext : public AppContext {
  public:
    HomeAppContext();

    void stateManagment() override;

  protected:
    int getMainPageId() override;
    void onPageChanged() override;
    bool isActiveWidget(const WidgetCursor &widgetCursor) override;
    void onPageTouch(const WidgetCursor &foundWidget, Event &touchEvent) override;
};

extern HomeAppContext g_homeAppContext;

void openApplication();
void closeApplication();

} // namespace home
} // namespace eez
