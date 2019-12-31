#include <stdint.h>

// options to control how MicroPython is built

#define MICROPY_QSTR_BYTES_IN_HASH  (1)
#define MICROPY_ALLOC_PATH_MAX      (512)
#define MICROPY_EMIT_X64            (0)
#define MICROPY_EMIT_THUMB          (0)
#define MICROPY_EMIT_INLINE_THUMB   (0)
#define MICROPY_COMP_MODULE_CONST   (0)
#define MICROPY_COMP_CONST          (0)
#define MICROPY_COMP_DOUBLE_TUPLE_ASSIGN (0)
#define MICROPY_COMP_TRIPLE_TUPLE_ASSIGN (0)
#define MICROPY_MEM_STATS           (0)
#define MICROPY_DEBUG_PRINTERS      (0)
#define MICROPY_ENABLE_GC           (1)
#define MICROPY_HELPER_REPL         (0)
#define MICROPY_HELPER_LEXER_UNIX   (0)
#define MICROPY_ENABLE_SOURCE_LINE  (1)
#define MICROPY_ENABLE_DOC_STRING   (0)
#define MICROPY_ERROR_REPORTING     (MICROPY_ERROR_REPORTING_TERSE)
#define MICROPY_BUILTIN_METHOD_CHECK_SELF_ARG (0)
#define MICROPY_PY_ASYNC_AWAIT (0)
#define MICROPY_PY_BUILTINS_BYTEARRAY (0)
#define MICROPY_PY_BUILTINS_DICT_FROMKEYS (0)
#define MICROPY_PY_BUILTINS_MEMORYVIEW (0)
#define MICROPY_PY_BUILTINS_ENUMERATE (0)
#define MICROPY_PY_BUILTINS_FROZENSET (0)
#define MICROPY_PY_BUILTINS_REVERSED (0)
#define MICROPY_PY_BUILTINS_SET     (0)
#define MICROPY_PY_BUILTINS_SLICE   (0)
#define MICROPY_PY_BUILTINS_PROPERTY (0)
#define MICROPY_PY_BUILTINS_STR_COUNT (0)
#define MICROPY_PY_BUILTINS_STR_OP_MODULO (0)
#define MICROPY_PY___FILE__         (0)
#define MICROPY_PY_GC               (0)
#define MICROPY_PY_ARRAY            (0)
#define MICROPY_PY_ATTRTUPLE        (0)
#define MICROPY_PY_COLLECTIONS      (0)
#define MICROPY_PY_MATH             (1)
#define MICROPY_PY_CMATH            (0)
#define MICROPY_PY_IO               (0)
#define MICROPY_PY_STRUCT           (1)
#define MICROPY_PY_SYS              (0)
#define MICROPY_CPYTHON_COMPAT      (0)
#define MICROPY_LONGINT_IMPL        (MICROPY_LONGINT_IMPL_NONE)
#define MICROPY_FLOAT_IMPL          (MICROPY_FLOAT_IMPL_FLOAT)
#define MICROPY_USE_INTERNAL_PRINTF (0)

#define MICROPY_PY_UTIME            (1)
#define MICROPY_PY_UTIME_MP_HAL     (1)

// type definitions for the specific machine

#define MICROPY_MAKE_POINTER_CALLABLE(p) ((void*)((mp_uint_t)(p) | 1))

#define UINT_FMT "%lu"
#define INT_FMT "%ld"

typedef int32_t mp_int_t; // must be pointer size
typedef uint32_t mp_uint_t; // must be pointer size
typedef long mp_off_t;

#define MP_PLAT_PRINT_STRN(str, len) ((void)mp_hal_stdout_tx_strn(str, len))

// extra built in names to add to the global namespace
#define MICROPY_PORT_BUILTINS \
    { MP_ROM_QSTR(MP_QSTR_open), MP_ROM_PTR(&mp_builtin_open_obj) },

#define MODULE_EEZ_ENABLED (1)

// We need to provide a declaration/definition of alloca()
#ifdef EEZ_PLATFORM_SIMULATOR_WIN32

#include <malloc.h>
#define MP_ENDIANNESS_LITTLE 1

#define NORETURN                    __declspec(noreturn)
#define MP_WEAK
#define MP_NOINLINE                 __declspec(noinline)
#define MP_LIKELY(x)                (x)
#define MP_UNLIKELY(x)              (x)
#define MICROPY_PORT_CONSTANTS      { "dummy", 0 } //can't have zero-sized array
#ifdef _WIN64
#define MP_SSIZE_MAX                _I64_MAX
#else
#define MP_SSIZE_MAX                _I32_MAX
#endif

// CL specific definitions

#ifndef __cplusplus
#define restrict
#define inline                      __inline
#define alignof(t)                  __alignof(t)
#endif
#define PATH_MAX                    MICROPY_ALLOC_PATH_MAX
#define S_ISREG(m)                  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)                  (((m) & S_IFMT) == S_IFDIR)
#ifdef _WIN64
#define SSIZE_MAX                   _I64_MAX
typedef __int64                     ssize_t;
#else
#define SSIZE_MAX                   _I32_MAX
typedef int                         ssize_t;
#endif
typedef mp_off_t                    off_t;


#else
#include <alloca.h>
#endif
