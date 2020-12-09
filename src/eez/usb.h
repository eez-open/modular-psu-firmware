/*
* EEZ Generic Firmware
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

#pragma once

#include <stdint.h>

namespace eez {
namespace usb {

void init();
void tick(uint32_t tickCount);

void selectUsbMode(int usbMode, int otgMode);
void selectUsbDeviceClass(int usbDeviceClass);

bool isVirtualComPortActive();
bool isMassStorageActive();

bool isOtgHostModeDetected();

} // usb
} // eez

#define USB_MODE_DISABLED 0
#define USB_MODE_DEVICE 1
#define USB_MODE_HOST 2
#define USB_MODE_OTG 3

#define USB_DEVICE_CLASS_VIRTUAL_COM_PORT 0
#define USB_DEVICE_CLASS_MASS_STORAGE_CLIENT 1
#define USB_HOST_CLASS_HID 2

extern "C" int g_usbDeviceClass;
extern "C" int g_usbMode;
extern "C" int g_otgMode;
extern "C" int g_selectedMassStorageDevice;
