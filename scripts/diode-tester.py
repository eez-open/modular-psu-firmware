from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, getOutputMode, getI, dlogTraceData

U_MAX = 40.0
I_SET = 0.01
TIME_STEP_MS = 20

diodeName = scpi('disp:input? "Diode name", TEXT, 20, ""')
if diodeName != None:
    uStep = scpi('disp:input? "U step", NUMBER, VOLT, 0.01, 1, 0.1')
    if uStep != None:
        uStep = float(uStep)

        try:
            scpi('DISP:INPUT? "Connect your diode on CH1", MENU, BUTTON, "Start"')
            scpi("OUTP 0")
            scpi("INST ch1")
            scpi("SENS:CURR:RANG MIN")
            scpi("VOLT 0")
            scpi("CURR " + str(I_SET))

            scpi("SENS:DLOG:TRAC:X:UNIT VOLT")
            scpi("SENS:DLOG:TRAC:X:STEP " + str(uStep))
            scpi("SENS:DLOG:TRAC:X:RANG:MAX " + str(U_MAX))
            scpi("SENS:DLOG:TRAC:Y1:UNIT AMPER")
            scpi("SENS:DLOG:TRAC:Y1:RANG:MAX " + str(I_SET))

            scpi('INIT:DLOG:TRACE "/Recordings/' + diodeName + '.dlog"')

            scpi("OUTP 1")

            scpi("DISP:WINDOW:DLOG")

            ch = 1

            t = ticks_ms()
            i = 0
            while True:
                u_set = i * uStep
                if u_set > U_MAX:
                    break

                setU(ch, u_set)
                #scpi("VOLT " + str(u_set))

                t = ticks_add(t, TIME_STEP_MS)
                sleep_ms(ticks_diff(t, ticks_ms()))

                curr = getI(ch)
                #curr = scpi("MEAS:CURR?")

                dlogTraceData(curr)
                #scpi("DLOG:TRACE:DATA " + scpi("MEAS:CURR?"))

                mode = getOutputMode(ch)
                #mode = scpi("OUTP:MODE?")

                print(u_set, curr, mode)

                if mode == "CC":
                    break
                # if curr > I_SET:
                #     break

                i = i + 1
        finally:
            scpi("ABOR:DLOG")
            scpi("OUTP 0")
