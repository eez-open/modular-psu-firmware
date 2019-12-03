#import ustruct
#from utime import ticks_ms, sleep_ms
from eez import scpi #, dlog

U_STEP = 0.1
U_MAX = 40.0
TIME_STEP_MS = 10
MEASURE_DELAY_MS = 5
I_SET = 0.001

scpi("inst ch1")
scpi("curr " + str(I_SET))
scpi("outp 1")

print("start")

#i_list = []
#t = ticks_ms()
i = 0
while True:
    u_set = i * U_STEP
    if u_set > U_MAX:
        break

    scpi("volt " + str(u_set))

    i = i + 1

    #t = ticks_add(t, MEASURE_DELAY_MS)
    #sleep_ms(ticks_diff(t, ticks_ms()))

    #if scpi("outp:mode?").startswith("CC"):
    #    break

    #i_list.add(scpi("curr?"))

     #t = ticks_add(t, (TIME_STEP_MS - MEASURE_DELAY_MS) * 1000)
    #sleep_ms(ticks_diff(t, ticks_ms()))

print("end")

# FILE_NAME_MAX_CHARS = 20
# fileName = scpi('query text, "Diode model", ' % FILE_NAME_MAX_CHARS)
# dlog(fileName, TIME_STEP_MS / 1000, i_list)
