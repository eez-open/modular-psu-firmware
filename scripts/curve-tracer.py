from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, getOutputMode, getI, dlogTraceData


TIME_STEP_MS = 10

Ugs_step = 1
Ugs_min = 1
Ugs_max = 4
CH1_I_SET = 0.01

Uds_step = 0.1
Uds_min = 0
Uds_max = 40
Id_max = 0.5 # Id,max = 1A

uBreakdown = None
try:
    scpi("*SAV 10")
    scpi("MEM:STAT:FREEze ON")

    scpi("INST:COUP:TRAC CGND")

    scpi("INST ch1")
    scpi("OUTP 0")
    scpi("VOLT 0")
    scpi("CURR " + str(CH1_I_SET))

    scpi("INST ch2")
    scpi("OUTP 0")
    scpi("VOLT 0")
    scpi("CURR " + str(Id_max))

    scpi("SENS:DLOG:TRAC:X:UNIT VOLT")
    scpi("SENS:DLOG:TRAC:X:STEP " + str(Uds_step))
    scpi("SENS:DLOG:TRAC:X:RANG:MAX " + str(Uds_max))
    scpi('SENS:DLOG:TRAC:X:LABel "Uds"')

    n = int((Ugs_max - Ugs_min) / Ugs_step + 1)
    m = int((Uds_max - Uds_min) / Uds_step + 1)

    for i in range(n):
        scpi("SENS:DLOG:TRAC:Y" + str(i+1) + ":UNIT AMPER")
        scpi("SENS:DLOG:TRAC:Y" + str(i+1) + ":RANG:MAX " + str(Id_max))
        scpi('SENS:DLOG:TRAC:Y' + str(i+1) + ':LABel "Ugs=' + str(Ugs_min + i * Ugs_step) + 'V"')

    scpi('INIT:DLOG:TRACE "/Recordings/curve_tracer.dlog"')

    scpi("INST ch1")
    scpi("OUTP 1")
    scpi("INST ch2")
    scpi("OUTP 1")

    scpi("DISP:WINDOW:DLOG")

    iMonValues = [0.0] * n

    t = ticks_ms()
    for j in range(m):
        Uds = Uds_min + j * Uds_step
        setU(2, Uds)
        
        for i in range(n):
            Ugs = Ugs_min + i * Ugs_step
            setU(1, Ugs)

            t = ticks_add(t, TIME_STEP_MS)
            sleep_ms(ticks_diff(t, ticks_ms()))

            iMonValues[i] = getI(2)
            # iMonValues[i] = 0.1 * i + Uds / 100 

            setU(1, 0)
            t = ticks_add(t, TIME_STEP_MS)
            sleep_ms(ticks_diff(t, ticks_ms()))

        dlogTraceData(iMonValues)
finally:
    scpi("ABOR:DLOG")
    scpi("*RCL 10")
    scpi("MEM:STAT:FREEze OFF")
