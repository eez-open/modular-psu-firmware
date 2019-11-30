#pragma warning( push )
#pragma warning( disable : 4200)

extern "C" {
#include "modeez.h"
#include <py/objtuple.h>
#include <py/runtime.h>
}

#pragma warning( pop ) 


static mp_obj_t TupleForRGB(uint8_t r, uint8_t g, uint8_t b) {
  mp_obj_tuple_t * t = static_cast<mp_obj_tuple_t *>(MP_OBJ_TO_PTR(mp_obj_new_tuple(3, NULL)));
  t->items[0] = MP_OBJ_NEW_SMALL_INT(r);
  t->items[1] = MP_OBJ_NEW_SMALL_INT(g);
  t->items[2] = MP_OBJ_NEW_SMALL_INT(b);
  return MP_OBJ_FROM_PTR(t);
}

mp_obj_t modeez_test(mp_obj_t param1, mp_obj_t param2, mp_obj_t param3) {
  return TupleForRGB(
    mp_obj_get_int(param1),
    mp_obj_get_int(param2),
    mp_obj_get_int(param3)
  );
}
