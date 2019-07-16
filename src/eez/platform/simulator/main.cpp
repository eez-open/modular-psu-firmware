#include <eez/system.h>

#ifdef __EMSCRIPTEN__
#include <stdio.h>
#include <emscripten.h>
#include <eez/apps/psu/psu.h>
#include <eez/apps/psu/serial_psu.h>

void eez_serial_put(int ch) {
    Serial.put(ch);
}

extern void eez_system_tick();

static int g_initialized = false;

// clang-format off
void mount_file_system() {
	EM_ASM(
		FS.mkdir("/eez_modular_firmware"); 
		FS.mount(IDBFS, {}, "/eez_modular_firmware");

		//Module.print("start file sync..");

		//flag to check when data are synchronized
		Module.syncdone = 0;

		FS.syncfs(true, function(err) {
			assert(!err);
			//Module.print("end file sync..");
			Module.syncdone = 1;
		});
	);
}
// clang-format on

void main_loop() {
    if (emscripten_run_script_int("Module.syncdone") == 1) {
        if (!g_initialized) {
            g_initialized = true;
            eez::boot();
        } else {
            eez_system_tick();

            while (1) {
                int ch = getchar();
                if (ch == 0 || ch == EOF) {
                    break;
                }
                eez_serial_put(ch);
            }

            // clang-format off
            EM_ASM(
                if (Module.syncdone) {
                    //Module.print("Start File sync..");
                    Module.syncdone = 0;
                    
                    FS.syncfs(false, function (err) {
                        assert(!err);
                        //Module.print("End File sync..");
                        Module.syncdone = 1;
                    });
                }
            );
            // clang-format on
        }
    }
}
#endif

int main(int, char **) {
#ifdef __EMSCRIPTEN__
    mount_file_system();
    emscripten_set_main_loop(main_loop, 0, 1);
#else
    eez::boot();
#endif
    return 0;
}
