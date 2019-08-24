/*
 * EEZ Middleware
 * Copyright (C) 2017-present, Envox d.o.o.
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

#include <eez/platform/simulator/events.h>

#include <SDL.h>

#include <eez/system.h>
#include <eez/modules/mcu/encoder.h>

namespace eez {
namespace platform {
namespace simulator {

int g_mouseX;
int g_mouseY;
int g_mouseButton1DownX;
int g_mouseButton1DownY;
bool g_mouseButton1IsPressed;

void readEvents() {
    int yMouseWheel = 0;
    bool mouseButton2IsUp = false;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == 1) {
                SDL_GetMouseState(&g_mouseX, &g_mouseY);
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    g_mouseButton1DownX = g_mouseX;
                    g_mouseButton1DownY = g_mouseY;
                    g_mouseButton1IsPressed = true;
                } else if (event.type == SDL_MOUSEBUTTONUP) {
                    g_mouseButton1IsPressed = false;
                }
            }
            if (event.button.button == 2) {
                if (event.type == SDL_MOUSEBUTTONUP) {
                    mouseButton2IsUp = true;
                }
            }
        } else if (event.type == SDL_MOUSEWHEEL) {
            yMouseWheel += event.wheel.y;
        }

        if (event.type == SDL_QUIT) {
            eez::shutdown();
        }
    }

    // for web simulator
    if (yMouseWheel >= 100 || yMouseWheel <= -100) {
        yMouseWheel /= 100;
    }

    mcu::encoder::write(yMouseWheel, mouseButton2IsUp);
}

} // namespace simulator
} // namespace platform
} // namespace eez