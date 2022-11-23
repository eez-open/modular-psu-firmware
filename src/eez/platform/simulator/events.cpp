/*
 * EEZ Middleware
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

#if defined(EEZ_PLATFORM_SIMULATOR)

#if EEZ_OPTION_GUI || !defined(EEZ_OPTION_GUI)

#include <math.h>

#if !defined(__EMSCRIPTEN__)
#include <SDL.h>
#endif

#include <eez/core/encoder.h>
#include <eez/core/keyboard.h>
#include <eez/core/os.h>

#include <eez/gui/gui.h>

#include <eez/platform/simulator/events.h>

namespace eez {

namespace platform {
namespace simulator {

int g_mouseX;
int g_mouseY;
bool g_mouseButton1IsPressed;

void readEvents() {
#if !defined(__EMSCRIPTEN__)
    int yMouseWheel = 0;
    bool mouseButton2IsUp = false;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
            if (event.button.button == 1) {
                if (event.type == SDL_MOUSEBUTTONDOWN) {
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
        } else if (event.type == SDL_KEYDOWN) {
            keyboard::onKeyboardEvent(&event.key);
        } else if (event.type == SDL_WINDOWEVENT)  {
            if (event.window.event == SDL_WINDOWEVENT_SHOWN || event.window.event == SDL_WINDOWEVENT_RESTORED) {
                eez::gui::refreshScreen();
            }
        }

        if (event.type == SDL_QUIT) {
            eez::shutdown();
        }
    }

    SDL_GetMouseState(&g_mouseX, &g_mouseY);

    // for web simulator
    if (yMouseWheel >= 100 || yMouseWheel <= -100) {
        yMouseWheel /= 100;
    }

#if OPTION_ENCODER
    mcu::encoder::write(yMouseWheel, mouseButton2IsUp);
#endif
#endif
}

bool isMiddleButtonPressed() {
#if !defined(__EMSCRIPTEN__)
    int x;
    int y;
	osDelay(1000);
    auto buttons = SDL_GetMouseState(&x, &y);
    return buttons & SDL_BUTTON(2) ? true : false;
#else
    return false;
#endif
}

} // namespace simulator
} // namespace platform
} // namespace eez

#if defined(__EMSCRIPTEN__)

EM_PORT_API(void) onPointerEvent(int x, int y, int pressed) {
    using namespace eez::platform::simulator;

    g_mouseX = x;
    g_mouseY = y;
    g_mouseButton1IsPressed = pressed;
}

EM_PORT_API(void) onMouseWheelEvent(double yMouseWheel, int clicked) {
#if OPTION_ENCODER
    if (yMouseWheel >= 100 || yMouseWheel <= -100) {
        yMouseWheel /= 100;
    }
    eez::mcu::encoder::write(round(yMouseWheel), clicked);
#endif
}

#endif

#endif

#endif // defined(EEZ_PLATFORM_SIMULATOR)
