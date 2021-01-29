# Determines diode reverse breakdown voltage

from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, getOutputMode, getI, dlogTraceData

I_SET = 0.01
TIME_STEP_MS = 20

diode_name = None
U_step = 0.1

def error(message):
    scpi('DISP:ERROR "' + message + '"')

def update_U_step():
    scpi('DISP:DIALog:DATA "U_step",FLOAT,VOLT,' + str(U_step))

def update_can_start():
    scpi('DISP:DIALog:DATA "can_start",INTEGER,' + ("1" if can_start() else "0"))

def input_diode_name():
    global diode_name
    value = scpi('disp:input? "Diode name",TEXT,1,20,"' + (diode_name if diode_name != None else "") + '"')
    if value != None:
        diode_name = value
        scpi('DISP:DIALog:DATA "diode_name",STRING,"' + diode_name + '"')
        update_can_start()

def input_U_step():
    global U_step
    value = scpi('DISP:INPUT? "U_step",NUMBER,VOLT,0.01,1,' + str(U_step))
    if value != None:
        U_step = float(value)
        update_U_step()

def can_start():
    return diode_name != None

def start():
    if not can_start():
        error("Enter diode name")
        return

    scpi("DISP:DIALog:CLOSe")

    uStep = U_step
    uBreakdown = None
    try:
        scpi("*SAV 10")
        scpi("MEM:STAT:FREEze ON")

        ch1Model = scpi("SYSTem:CHANnel:MODel? ch1")
        ch2Model = scpi("SYSTem:CHANnel:MODel? ch2")
        if ch1Model.startswith("DCP405") and ch2Model.startswith("DCP405"):
            scpi("INST:COUP:TRAC SER")
        else:
            scpi("INST:COUP:TRAC NONE")

        scpi("INST ch1")

        uMax = float(scpi("VOLT? MAX"))

        scpi("OUTP 0")

        scpi("SENS:CURR:RANG LOW")
        scpi("VOLT 0")
        scpi("CURR " + str(I_SET))


        scpi("SENS:DLOG:CLE")

        scpi('SENS:DLOG:TRAC:REM "Diode name = ' + diode_name + '; U step = ' + str(U_step) + '"')

        scpi("SENS:DLOG:TRAC:X:UNIT VOLT")
        scpi("SENS:DLOG:TRAC:X:STEP " + str(U_step))
        scpi("SENS:DLOG:TRAC:X:RANG:MAX " + str(uMax))
        scpi('SENS:DLOG:TRAC:X:LABel "Uset"')
        scpi("SENS:DLOG:TRAC:Y1:UNIT AMPER")
        scpi("SENS:DLOG:TRAC:Y1:RANG:MAX " + str(I_SET))
        scpi('SENS:DLOG:TRAC:Y1:LABel "Imon"')
        scpi('INIT:DLOG:TRACE "/Recordings/' + diode_name + '.dlog"')

        scpi("OUTP 1")

        scpi("DISP:WINDOW:DLOG")

        ch = 1
        t = ticks_ms()
        i = 0
        while True:
            uSet = i * U_step
            
            if uSet > uMax:
                break

            setU(ch, uSet)
            #scpi("VOLT " + str(uSet))

            t = ticks_add(t, TIME_STEP_MS)
            sleep_ms(ticks_diff(t, ticks_ms()))

            iMon = getI(ch)
            #iMon = scpi("MEAS:CURR?")

            dlogTraceData(iMon)
            #scpi("DLOG:TRACE:DATA " + scpi("MEAS:CURR?"))

            mode = getOutputMode(ch)
            #mode = scpi("OUTP:MODE?")

            #print(uSet, iMon, mode)

            #if mode == "CC":
            #    break
            if iMon >= I_SET:
                uBreakdown = uSet
                break

            i = i + 1
    finally:
        scpi("ABOR:DLOG")
        scpi("*RCL 10")
        scpi("MEM:STAT:FREEze OFF")

    if uBreakdown != None:
        scpi('DISP:INPUT? "Breakdown voltage is ' + str(round(uBreakdown, 2)) + 'V", MENU, BUTTON, "Close"')
    else:
        scpi('DISP:INPUT? "Breakdown voltage not found", MENU, BUTTON, "Close"')    

scpi("DISP:DIALog:OPEN \"/Scripts/Diode Tester.res\"")
update_U_step()
while True:
    action = scpi("DISP:DIALog:ACTIon?")
    if action == "input_diode_name":
        input_diode_name()
    elif action == "input_U_step":
        input_U_step()
    elif action == "start":
        start()
        break
    elif action == "close" or action == 0:
        scpi("DISP:DIALog:CLOSe")
        break

