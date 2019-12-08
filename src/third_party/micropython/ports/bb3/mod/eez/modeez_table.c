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

#include "modeez.h"

STATIC MP_DEFINE_CONST_FUN_OBJ_1(modeez_scpi_obj, modeez_scpi);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(modeez_getU_obj, modeez_getU);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(modeez_setU_obj, modeez_setU);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(modeez_getI_obj, modeez_getI);
STATIC MP_DEFINE_CONST_FUN_OBJ_2(modeez_setI_obj, modeez_setI);
STATIC MP_DEFINE_CONST_FUN_OBJ_1(modeez_getOutputMode_obj, modeez_getOutputMode);
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(modeez_dlogTraceData_obj, 1, 4, modeez_dlogTraceData);

STATIC const mp_rom_map_elem_t modeez_module_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_eez) },
  { MP_ROM_QSTR(MP_QSTR_scpi), (mp_obj_t)&modeez_scpi_obj },
  { MP_ROM_QSTR(MP_QSTR_getU), (mp_obj_t)&modeez_getU_obj },
  { MP_ROM_QSTR(MP_QSTR_setU), (mp_obj_t)&modeez_setU_obj },
  { MP_ROM_QSTR(MP_QSTR_getI), (mp_obj_t)&modeez_getI_obj },
  { MP_ROM_QSTR(MP_QSTR_setI), (mp_obj_t)&modeez_setI_obj },
  { MP_ROM_QSTR(MP_QSTR_getOutputMode), (mp_obj_t)&modeez_getOutputMode_obj },
  { MP_ROM_QSTR(MP_QSTR_dlogTraceData), (mp_obj_t)&modeez_dlogTraceData_obj },
};

STATIC MP_DEFINE_CONST_DICT(modeez_module_globals, modeez_module_globals_table);

const mp_obj_module_t modeez_module = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t*)&modeez_module_globals,
};

// Register the module to make it available in Python
MP_REGISTER_MODULE(MP_QSTR_eez, modeez_module, MODULE_EEZ_ENABLED);