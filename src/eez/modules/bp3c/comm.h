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

#include <stdint.h>

#pragma once

namespace eez {
namespace bp3c {
namespace comm {

bool masterSynchro(int slotIndex);

enum TransferResult {
    TRANSFER_STATUS_OK,
    TRANSFER_STATUS_ERROR,
    TRANSFER_STATUS_BUSY,
    TRANSFER_STATUS_TIMEOUT,
    TRANSFER_STATUS_CRC_ERROR
};

TransferResult transfer(int slotIndex, uint8_t *output, uint8_t *input, uint32_t bufferSize);
TransferResult transferDMA(int slotIndex, uint8_t *output, uint8_t *input, uint32_t bufferSize);

} // namespace comm
} // namespace bp3c
} // namespace eez
