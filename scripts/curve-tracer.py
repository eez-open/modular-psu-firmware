from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, getOutputMode, getI, dlogTraceData

TIME_STEP_MS = 10

# Ch1 G-S
Ugs_min = 1
Ugs_max = 10
Ugs_step = 1
Ig = 0.01

# Ch2 D-S
Uds_min = 0
Uds_max = 40
Uds_step = 0.1
Id_max = 0.5

try:
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
    scpi("SENS:DLOG:TRAC:X:STEP " + str(Uds_step))
    scpi("SENS:DLOG:TRAC:X:RANG:MAX " + str(Uds_max))
    scpi('SENS:DLOG:TRAC:X:LABel "Uds"')

    num_ugs_steps = int((Ugs_max - Ugs_min) / Ugs_step + 1)
    num_uds_steps = int((Uds_max - Uds_min) / Uds_step + 1)

    for ugs_step_counter in range(num_ugs_steps):
        scpi("SENS:DLOG:TRAC:Y" + str(ugs_step_counter+1) + ":UNIT AMPER")
        scpi("SENS:DLOG:TRAC:Y" + str(ugs_step_counter+1) + ":RANG:MAX " + str(Id_max))
        scpi('SENS:DLOG:TRAC:Y' + str(ugs_step_counter+1) + ':LABel "Ugs=' + str(Ugs_min + ugs_step_counter * Ugs_step) + 'V"')

    scpi('INIT:DLOG:TRACE "/Recordings/curve_tracer.dlog"')

    scpi("INST ch1")
    scpi("OUTP 1")
    scpi("INST ch2")
    scpi("OUTP 1")

    scpi("DISP:WINDOW:DLOG")

    iMonValues = [0.0] * num_ugs_steps

    t = ticks_ms()
    for uds_step_counter in range(num_uds_steps):
        Uds = Uds_min + uds_step_counter * Uds_step
        setU(2, Uds)
        
        for ugs_step_counter in range(num_ugs_steps):
            Ugs = Ugs_min + ugs_step_counter * Ugs_step
            setU(1, Ugs)

            t = ticks_add(t, TIME_STEP_MS)
            sleep_ms(ticks_diff(t, ticks_ms()))

            iMonValues[ugs_step_counter] = getI(2)
            # iMonValues[ugs_step_counter] = 0.1 * ugs_step_counter + Uds / 100 

            setU(1, 0)

            t = ticks_add(t, TIME_STEP_MS)
            sleep_ms(ticks_diff(t, ticks_ms()))

        dlogTraceData(iMonValues)
finally:
    scpi("ABOR:DLOG")
    scpi("*RCL 10")
    scpi("MEM:STAT:FREEze OFF")
