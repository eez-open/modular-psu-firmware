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

STATIC const mp_rom_map_elem_t modeez_module_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_eez) },
  { MP_ROM_QSTR(MP_QSTR_scpi), (mp_obj_t)&modeez_scpi_obj },
};

STATIC MP_DEFINE_CONST_DICT(modeez_module_globals, modeez_module_globals_table);

const mp_obj_module_t modeez_module = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t*)&modeez_module_globals,
};

// Register the module to make it available in Python
MP_REGISTER_MODULE(MP_QSTR_eez, modeez_module, MODULE_EEZ_ENABLED);