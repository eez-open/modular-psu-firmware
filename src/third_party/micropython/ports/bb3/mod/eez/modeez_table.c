#include "modeez.h"

STATIC MP_DEFINE_CONST_FUN_OBJ_3(modeez_test_obj, modeez_test);

STATIC const mp_rom_map_elem_t modeez_module_globals_table[] = {
  { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_eez) },
  { MP_ROM_QSTR(MP_QSTR_test), (mp_obj_t)&modeez_test_obj },
};

STATIC MP_DEFINE_CONST_DICT(modeez_module_globals, modeez_module_globals_table);

const mp_obj_module_t modeez_module = {
  .base = { &mp_type_module },
  .globals = (mp_obj_dict_t*)&modeez_module_globals,
};

// Register the module to make it available in Python
MP_REGISTER_MODULE(MP_QSTR_eez, modeez_module, MODULE_EEZ_ENABLED);