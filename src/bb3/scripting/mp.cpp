/*
* EEZ Generic Firmware
* Copyright (C) 2018-present, Envox d.o.o.
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

#include <stdio.h>

#include <bb3/psu/gui/psu.h>

#include <eez/fs/fs.h>

#include <bb3/memory.h>
#include <bb3/scripting/scripting.h>
#include <bb3/scripting/mp.h>
#include <bb3/scripting/internal.h>
#include <bb3/scripting/scpi_context.h>
#include <bb3/scripting/thread.h>

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4200)
#endif

extern "C" {
#include "py/compile.h"
#include "py/runtime.h"
#include "py/gc.h"
#include "py/stackctrl.h"
}

#ifdef _MSC_VER
#pragma warning( pop )
#endif

namespace eez {
namespace scripting {

static char *g_scriptSource;
static size_t g_scriptSourceLength;

static const size_t MP_GC_MEMORY_SIZE = 512 * 1024;
static uint8_t *g_mpGCMemory;

void startMpScript() {
	char scriptName[64];
	getFileName(g_scriptPath, scriptName, sizeof(scriptName));
	InfoTrace("Script started: %s\n", scriptName);

	// this version reinitialise MP every time

	volatile char dummy;
	mp_stack_set_top((void *)&dummy);
	gc_init(g_mpGCMemory, g_mpGCMemory + MP_GC_MEMORY_SIZE);
	mp_init();

	bool wasException = false;

	nlr_buf_t nlr;
	if (nlr_push(&nlr) == 0) {
		mp_lexer_t *lex = mp_lexer_new_from_str_len(MP_QSTR__lt_stdin_gt_, g_scriptSource, g_scriptSourceLength, 0);
		qstr source_name = lex->source_name;
		mp_parse_tree_t parse_tree = mp_parse(lex, MP_PARSE_FILE_INPUT);
		mp_obj_t module_fun = mp_compile(&parse_tree, source_name/*, MP_EMIT_OPT_NONE*/, true);
		mp_call_function_0(module_fun);
		nlr_pop();
	} else {
		// uncaught exception
		mp_obj_print_exception(&mp_plat_print, (mp_obj_t)nlr.ret_val);
		onUncaughtScriptExceptionHook();
		wasException = true;
	}

	gc_sweep_all();
	mp_deinit();

    afterScriptCleanup(wasException);

	setStateIdle();

	InfoTrace("Script ended: %s\n", scriptName);
}

bool executeScpiFromMP(const char *commandOrQueryText, const char **resultText, size_t *resultTextLen) {
	// DebugTrace("> %s\n", commandOrQueryText);

	bool executeInLowPriorityThread = true;
	if (startsWithNoCase(commandOrQueryText, "DISP:INPUT?") || startsWithNoCase(commandOrQueryText, "DISP:DIALOG:ACTION?")) {
		executeInLowPriorityThread = false;
	}

	if (executeInLowPriorityThread) {
		executeScpi(commandOrQueryText, true);
		if (!waitScpiResult()) {
			static char g_scpiError[48];
			snprintf(g_scpiError, sizeof(g_scpiError), "SCPI timeout");
			mp_raise_ValueError(g_scpiError);
		}
	} else {
		executeScpi(commandOrQueryText, false);
	}

	int err;
	if (!getLatestScpiResult(resultText, resultTextLen, &err)) {
		static char g_scpiError[48];
		snprintf(g_scpiError, sizeof(g_scpiError), "SCPI error %d, \"%s\"", err, SCPI_ErrorTranslate(err));
		mp_raise_ValueError(g_scpiError);
	}

	return true;
}

bool loadMpScript(int *err) {
	uint32_t fileSize;
	uint32_t bytesRead;

	eez::File file;
	if (!file.open(g_scriptPath, FILE_OPEN_EXISTING | FILE_READ)) {
		if (err) {
			*err = SCPI_ERROR_FILE_NOT_FOUND;
		} else {
			generateError(SCPI_ERROR_FILE_NOT_FOUND);
		}
		goto ErrorNoClose;
	}

	fileSize = file.size();

	g_scriptSource = (char *)alloc(fileSize, 0x186d4c0e);
	if (!g_scriptSource) {
		if (err) {
			*err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
		} else {
			generateError(SCPI_ERROR_OUT_OF_DEVICE_MEMORY);
		}
		goto Error;
	}

	bytesRead = file.read(g_scriptSource, fileSize);

	file.close();

	if (bytesRead != fileSize) {
		if (err) {
			*err = SCPI_ERROR_MASS_STORAGE_ERROR;
		} else {
			generateError(SCPI_ERROR_MASS_STORAGE_ERROR);
		}

		goto Error;
	}

	g_scriptSourceLength = fileSize;

	g_mpGCMemory = (uint8_t *)alloc(MP_GC_MEMORY_SIZE, 0x77250f2d);
	if (!g_mpGCMemory) {
		if (err) {
			*err = SCPI_ERROR_OUT_OF_DEVICE_MEMORY;
		} else {
			generateError(SCPI_ERROR_OUT_OF_DEVICE_MEMORY);
		}

		goto ErrorNoClose;
	}

	startMpScriptInScriptingThread();

	return true;

Error:
	file.close();

ErrorNoClose:
	afterScriptCleanup(true);

	setStateIdle();

	return false;
}


void freeMpMemory() {
	if (g_scriptSource) {
		free(g_scriptSource);
		g_scriptSource = nullptr;
	}

	if (g_mpGCMemory) {
		free(g_mpGCMemory);
		g_mpGCMemory = nullptr;
	}
}

} // scripting
} // eez
