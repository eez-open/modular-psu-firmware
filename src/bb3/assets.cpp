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

#include <eez/gui/gui.h>

#include <bb3/fs/fs.h>

#include <scpi/scpi.h>

namespace eez {
namespace gui {

bool loadExternalAssets(const char *filePath, int *err) {
	unloadExternalAssets();

    eez::File file;
    if (!file.open(filePath, FILE_OPEN_EXISTING | FILE_READ)) {
        if (err) {
            *err = SCPI_ERROR_FILE_NAME_NOT_FOUND;
        }
        return false;
    }

    uint32_t fileSize = file.size();

    if (fileSize == 0) {
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

	uint8_t *fileData = (uint8_t *)alloc(((fileSize + 3) / 4) * 4, 202);
    if (!fileData) {
        if (err) {
            *err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
        }
        return false;
    }

    uint32_t bytesRead = file.read(fileData, fileSize);
    file.close();

    if (bytesRead != fileSize) {
		free(fileData);
        if (err) {
            *err = SCPI_ERROR_MASS_STORAGE_ERROR;
        }
        return false;
    }

	auto header = (Header *)fileData;

	if (header->tag != HEADER_TAG) {
		free(fileData);
		if (err) {
			*err = SCPI_ERROR_INVALID_BLOCK_DATA;
		}
		return false;
	}

// disable warning: offsetof within non-standard-layout type ... is conditionally-supported [-Winvalid-offsetof]
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-offsetof"
#endif

	auto decompressedDataOffset = offsetof(Assets, pages);

 #ifdef __GNUC__
 #pragma GCC diagnostic pop
 #endif

	size_t externalAssetsSize = decompressedDataOffset + header->decompressedSize;
	g_externalAssets = (Assets *)alloc(externalAssetsSize, 301);
	if (!g_externalAssets) {
		free(g_externalAssets);
		g_externalAssets = nullptr;

		if (err) {
			*err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
		}

		return false;
	}

	g_externalAssets->external = true;

	auto result = decompressAssetsData(fileData, fileSize, g_externalAssets, externalAssetsSize, err);

	free(fileData);

	if (!result) {
		free(g_externalAssets);
		g_externalAssets = nullptr;
		return false;
	}

	return true;
}

} // namespace gui
} // namespace eez
