import math
from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, getOutputMode, getI, dlogTraceData

TIME_ON_MS = 15
TIME_OFF_MS = 6

# Ch1 G-S
Ugs = [3, 3.5, 4, 4.5, 6, 8, 12]
Ig = 5.0

# Ch2 D-S
Uds_min = 0.1
NUM_U_DS_STEPS = 400

SIMULATOR = 0
if SIMULATOR:
    TIME_ON_MS = 10
    TIME_OFF_MS = 0
    Ugs = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    Ig = 5.0
    
def start(deviceName, Uds_max, Id_max):
    try:
        #Uds_step = Uds_max / NUM_U_DS_STEPS
        Uds_logMin = math.log(Uds_min) / math.log(10)
        Uds_logMax = math.log(Uds_max) / math.log(10)
        Uds_step = (Uds_logMax - Uds_logMin) / (NUM_U_DS_STEPS - 1)

        scpi("*SAV 10")
        scpi("MEM:STAT:FREEze ON")

        scpi("INST:COUP:TRAC CGND")

        scpi("INST ch1")
        scpi("OUTP 0")
        scpi("VOLT 0")
        scpi("CURR " + str(Ig))

        scpi("INST ch2")
        scpi("OUTP 0")
        scpi("VOLT 0")
        scpi("CURR " + str(Id_max))

        scpi("SENS:DLOG:TRAC:X:UNIT VOLT")
        scpi("SENS:DLOG:TRAC:X:RANG:MIN " + str(Uds_logMin))
        scpi("SENS:DLOG:TRAC:X:RANG:MAX " + str(Uds_logMax))
        scpi("SENS:DLOG:TRAC:X:STEP " + str(Uds_step))
        scpi("SENS:DLOG:TRAC:X:SCALE LOG")
        scpi('SENS:DLOG:TRAC:X:LABel "Uds"')

        num_ugs_steps = len(Ugs)

        scpi("SENS:DLOG:TRAC:Y:UNIT AMPER")
        scpi("SENS:DLOG:TRAC:Y:RANG:MIN 0")
        scpi("SENS:DLOG:TRAC:Y:RANG:MAX " + str(Id_max * 1.1))
        scpi('SENS:DLOG:TRAC:Y:LABel "Id"')
        scpi('SENS:DLOG:TRAC:Y:SCALe LOG')

        for ugs_step_counter in range(num_ugs_steps):
            scpi('SENS:DLOG:TRAC:Y' + str(ugs_step_counter+1) + ':LABel "Ugs=' + str(Ugs[ugs_step_counter]) + 'V"')

        scpi('INIT:DLOG:TRACE "/Recordings/' + deviceName + '.dlog"')

        scpi("INST ch1")
        scpi("OUTP 1")
        scpi("INST ch2")
        scpi("OUTP 1")

        if SIMULATOR:
            scpi("SIMU:LOAD:STATE ON,CH2")

        scpi("DISP:WINDOW:DLOG")

        iMonValues = [0.0] * num_ugs_steps

        t = ticks_ms()
        for uds_step_counter in range(NUM_U_DS_STEPS):
            Uds = math.pow(10, Uds_logMin + uds_step_counter * Uds_step)

            print(Uds)

            setU(2, Uds)
            
            for ugs_step_counter in range(num_ugs_steps):
                if SIMULATOR:
                    if Uds < 10:
                        scpi("SIMU:LOAD " + str(20 / Ugs[ugs_step_counter]) + ",CH2")
                    else:
                        scpi("SIMU:LOAD " + str((20 / 10) * Uds / Ugs[ugs_step_counter]) + ",CH2")

                setU(1, Ugs[ugs_step_counter])

                t = ticks_add(t, TIME_ON_MS)
                sleep_ms(ticks_diff(t, ticks_ms()))

                iMonValues[ugs_step_counter] = getI(2)

                setU(1, 0)

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
        Uds_max = scpi('DISP:INPUT? "Uds,max", NUMBER, VOLT, 1.0, 40.0, 15.0')
        if Uds_max != None:
            Id_max = scpi('DISP:INPUT? "Id,max", NUMBER, AMPER, 0.001, 5.0, 2.0')
            if Id_max != None:
                start(deviceName, float(Uds_max), float(Id_max))
else:
    scpi('DISP:INPUT? "Requires DCP405 or DCP405B on Ch1 and Ch2", MENU, BUTTON, "Close"')
