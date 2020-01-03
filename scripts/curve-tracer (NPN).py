import math
from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, setI, getOutputMode, getI, dlogTraceData

TIME_ON_MS = 30
TIME_OFF_MS = 0

# Ch1 B-E
Ib = [50e-6, 100e-6, 150e-6, 200e-6, 250e-6, 300e-6, 350e-6, 400e-6]
Ube = 10.0

# Ch2 C-E
Uce_min = 0.1
NUM_U_DS_STEPS = 400

def start(deviceName, Uce_max, Ic_max):
    try:
        Uce_step = Uce_max / NUM_U_DS_STEPS

        scpi("*SAV 10")
        scpi("MEM:STAT:FREEze ON")

        scpi("INST:COUP:TRAC CGND")

        scpi("INST ch1")
        scpi("OUTP 0")
        scpi("VOLT " + str(Ube))
        scpi("CURR 0")
        scpi("OUTP:DPR 1")
        scpi("SENS:CURR:RANG 50mA")

        scpi("INST ch2")
        scpi("OUTP 0")
        scpi("VOLT 0")
        scpi("CURR " + str(Ic_max))
        scpi("SENS:CURR:RANG DEFault")

        scpi('SENS:DLOG:TRAC:COMMent "Device name = ' + deviceName + '; Uce,max = ' + str(Uce_max) + '; Ic,max = ' + str(Ic_max) + '"')

        scpi("SENS:DLOG:TRAC:X:UNIT VOLT")
        scpi("SENS:DLOG:TRAC:X:RANG:MIN " + str(Uce_min))
        scpi("SENS:DLOG:TRAC:X:RANG:MAX " + str(Uce_max))
        scpi("SENS:DLOG:TRAC:X:STEP " + str(Uce_step))
        scpi("SENS:DLOG:TRAC:X:SCALE LIN")
        scpi('SENS:DLOG:TRAC:X:LABel "Uce"')

        num_ib_steps = len(Ib)

        scpi("SENS:DLOG:TRAC:Y:UNIT AMPER")
        scpi("SENS:DLOG:TRAC:Y:RANG:MIN 0")
        scpi("SENS:DLOG:TRAC:Y:RANG:MAX " + str(Ic_max * 1.1))
        scpi('SENS:DLOG:TRAC:Y:LABel "Ic"')

        for ib_step_counter in range(num_ib_steps):
            scpi('SENS:DLOG:TRAC:Y' + str(ib_step_counter+1) + ':LABel "Ib=' + str(Ib[ib_step_counter] * 1E6) + 'uA"')

        scpi('INIT:DLOG:TRACE "/Recordings/' + deviceName + '.dlog"')

        scpi("INST ch1")
        scpi("OUTP 1")
        scpi("INST ch2")
        scpi("OUTP 1")

        scpi("DISP:WINDOW:DLOG")

        iMonValues = [0.0] * num_ib_steps

        t = ticks_ms()
        for uce_step_counter in range(NUM_U_DS_STEPS):
            Uce = Uce_min + uce_step_counter * Uce_step

            setU(2, Uce)
            
            for ib_step_counter in range(num_ib_steps):
                setI(1, Ib[ib_step_counter])

                t = ticks_add(t, TIME_ON_MS)
                sleep_ms(ticks_diff(t, ticks_ms()))

                iMonValues[ib_step_counter] = getI(2)

                if TIME_OFF_MS > 0:
                    setI(1, 0)
                    t = ticks_add(t, TIME_OFF_MS)
                    sleep_ms(ticks_diff(t, ticks_ms()))

            dlogTraceData(iMonValues)
    finally:
        scpi("ABOR:DLOG")
        scpi("*RCL 10")
        scpi("MEM:STAT:FREEze OFF")

ch1Model = scpi("SYSTem:CHANnel:MODel? ch1")
ch2Model = scpi("SYSTem:CHANnel:MODel? ch2")
if ch1Model.startswith("DCP405") and ch2Model.startswith("DCP405"):
    deviceName = scpi('DISP:INPUT? "Device name", TEXT, 1, 20, ""')
    if deviceName != None:
        Uce_max = scpi('DISP:INPUT? "Uce,max", NUMBER, VOLT, 1.0, 40.0, 15.0')
        if Uce_max != None:
            Ic_max = scpi('DISP:INPUT? "Ic,max", NUMBER, AMPER, 0.001, 5.0, 0.2')
            if Ic_max != None:
                start(deviceName, float(Uce_max), float(Ic_max))
else:
    scpi('DISP:INPUT? "Requires DCP405 or DCP405B on Ch1 and Ch2", MENU, BUTTON, "Close"')
