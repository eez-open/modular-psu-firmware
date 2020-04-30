# Plot N-type device current transfer characteristic
import math
from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, setI, getOutputMode, getI, dlogTraceData

TIME_STEP_MS = 15

# Ch1 B-E
Ib = 3E-3
Ube_min = 0.0
Ube_max = 1.2
NUM_U_BE_STEPS = 400

# Ch2 C-E
Ic_max = 200E-3
Uce = 5

def start(deviceName):
    try:
        Ube_step = (Ube_max - Ube_min) / NUM_U_BE_STEPS

        scpi("*SAV 10")
        scpi("MEM:STAT:FREEze ON")

        scpi("INST:COUP:TRAC CGND")

        scpi("INST ch1")
        scpi("OUTP 0")
        scpi("CURR " + str(Ib))
        scpi("VOLT 0")
        scpi("OUTP:DPR 1")
        scpi("SENS:CURR:RANG 50mA")

        scpi("INST ch2")
        scpi("OUTP 0")
        scpi("VOLT " + str(Uce))
        scpi("CURR " + str(Ic_max))
        scpi("SENS:CURR:RANG DEFault")

        scpi('SENS:DLOG:TRAC:REM "NPN Transfer curve for ' + deviceName + '"')

        scpi("SENS:DLOG:TRAC:X:UNIT VOLT")
        scpi("SENS:DLOG:TRAC:X:RANG:MIN " + str(Ube_min + Ube_step))
        scpi("SENS:DLOG:TRAC:X:RANG:MAX " + str(Ube_max))
        scpi("SENS:DLOG:TRAC:X:STEP " + str(Ube_step))
        scpi("SENS:DLOG:TRAC:X:SCALE LIN")
        scpi('SENS:DLOG:TRAC:X:LABel "Ube"')

        scpi("SENS:DLOG:TRAC:Y1:UNIT AMPER")
        scpi("SENS:DLOG:TRAC:Y1:RANG:MIN 0")
        scpi("SENS:DLOG:TRAC:Y1:RANG:MAX " + str(Ic_max * 1.1))
        scpi('SENS:DLOG:TRAC:Y1:LABel "Ic"')

        scpi('INIT:DLOG:TRACE "/Recordings/' + deviceName + '-transfer.dlog"')

        scpi("INST ch1")
        scpi("OUTP 1")
        scpi("INST ch2")
        scpi("OUTP 1")

        scpi("DISP:WINDOW:DLOG")

        t = ticks_ms()
        for ube_step_counter in range(NUM_U_BE_STEPS):
            Ube = Ube_min + (ube_step_counter + 1) * Ube_step

            setU(1, Ube)
            
            t = ticks_add(t, TIME_STEP_MS)
            sleep_ms(ticks_diff(t, ticks_ms()))

            dlogTraceData(getI(2))
    finally:
        scpi("ABOR:DLOG")
        scpi("*RCL 10")
        scpi("MEM:STAT:FREEze OFF")

ch1Model = scpi("SYSTem:CHANnel:MODel? ch1")
ch2Model = scpi("SYSTem:CHANnel:MODel? ch2")
if ch1Model.startswith("DCP405") and ch2Model.startswith("DCP405"):
    deviceName = scpi('DISP:INPUT? "Device name", TEXT, 1, 20, ""')
    if deviceName != None:
        start(deviceName)
else:
    scpi('DISP:INPUT? "Requires DCP405 on Ch1 and Ch2", MENU, BUTTON, "Close"')
