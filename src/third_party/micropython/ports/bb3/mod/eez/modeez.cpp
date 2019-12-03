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

#include <eez/mp.h>

#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4200)
#endif

extern "C" {
#include "modeez.h"
#include <py/objtuple.h>
#include <py/runtime.h>
}

#ifdef _MSC_VER
#pragma warning( pop ) 
#endif

mp_obj_t modeez_scpi(mp_obj_t commandOrQueryText) {
    const char *resultText;
    size_t resultTextLen;
    if (!eez::mp::scpi(mp_obj_str_get_str(commandOrQueryText), &resultText, &resultTextLen)) {
        return mp_const_false;
    }

    if (resultTextLen == 0) {
        return mp_const_none;
    }

    return mp_obj_new_str(resultText, resultTextLen);
}
