# Plot output characteristic for BJTs and MOSFETs

import math
from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, setI, getOutputMode, getI, dlogTraceData

TYPE_NPN = 0
TYPE_PNP = 1
TYPE_N_CH = 2
TYPE_P_CH = 3

deviceType = TYPE_NPN

deviceName = None

Uce_max = 15
Ic_max = 0.2

Uds_max = 15
Id_max = 2

def error(message):
    scpi('DISP:ERROR "' + message + '"')

def updateData(name, unit, value):
    scpi('DISP:DIALog:DATA "' + name + '",FLOAT,' + unit + ',' + str(value))

def inputVoltage(name, voltage, min, max):
    value = scpi('DISP:INPUT? "' + name + '",NUMBER,VOLT,' + str(min) + ',' + str(max) + ',' + str(voltage))
    if value != None:
        voltage = float(value)
        updateData(name, 'VOLT', voltage)
    return voltage

def inputAmperage(name, amperage, min, max):
    value = scpi('DISP:INPUT? "' + name + '",NUMBER,AMPER,' + str(min) + ',' + str(max) + ',' + str(amperage))
    if value != None:
        amperage = float(value)
        updateData(name, 'AMPER', amperage)
    return amperage
    
def select_device_type():
    global deviceType
    value = scpi('DISP:SELECT? ' + str(deviceType + 1) + ',"NPN BJT","PNP BJT","N-Ch MOSFET","P-Ch MOSFET"')
    if value != None and value != 0:
        deviceType = value - 1
        scpi("DISP:DIALog:DATA \"device_type\",INTEGER," + str(deviceType))

def input_device_name():
    global deviceName
    value = scpi('disp:input? "Device name",TEXT,1,20,"' + (deviceName if deviceName != None else "") + '"')
    if value != None:
        deviceName = value
        scpi('DISP:DIALog:DATA "device_name",STRING,"' + deviceName + '"')
        scpi('DISP:DIALog:DATA "can_start",INTEGER,1')

def input_Uce_max():
    global Uce_max
    Uce_max = inputVoltage("Uce_max", Uce_max, 1.0, 40.0)

def input_Ic_max():
    global Ic_max
    Ic_max = inputAmperage("Ic_max", Ic_max, 0.001, 5.0)

def input_Uds_max():
    global Uds_max
    Uds_max = inputVoltage("Uds_max", Uds_max, 1.0, 40.0)

def input_Id_max():
    global Id_max
    Id_max = inputAmperage("Id_max", Id_max, 0.001, 5.0)

def start():
    if deviceName == None:
        error("Enter device name")
        return

    scpi("DISP:DIALog:CLOSe")

    try:
        scpi("*SAV 10")
        scpi("MEM:STAT:FREEze ON")

        if deviceType == TYPE_NPN:
            start_BJT(True)
        elif deviceType == TYPE_PNP:
            start_BJT(False)
        elif deviceType == TYPE_N_CH:
            start_MOSFET(True)
        elif deviceType == TYPE_P_CH:
            start_MOSFET(False)
    finally:
        scpi("ABOR:DLOG")
        scpi("*RCL 10")
        scpi("MEM:STAT:FREEze OFF")

def start_BJT(cgnd):
    global deviceName, Uce_max, Ic_max

    TIME_ON_MS = 30
    TIME_OFF_MS = 0

    # Ch1 B-E
    Ib = [50e-6, 100e-6, 150e-6, 200e-6, 250e-6, 300e-6, 350e-6, 400e-6]
    Ube = 10.0

    # Ch2 C-E
    Uce_min = 0.1
    NUM_U_DS_STEPS = 400

    Uce_step = Uce_max / NUM_U_DS_STEPS

    if cgnd:
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
    scpi("SENS:CURR:RANG BEST")

    scpi('SENS:DLOG:TRAC:REM "Device name = ' + deviceName + '; Uce,max = ' + str(Uce_max) + '; Ic,max = ' + str(Ic_max) + '"')

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

def start_MOSFET(cgnd):
    global deviceName, Uds_max, Id_max

    TIME_ON_MS = 15
    TIME_OFF_MS = 6

    # Ch1 G-S
    Ugs = [3.5, 4, 4.5, 5, 5.5, 6]
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

    #Uds_step = Uds_max / NUM_U_DS_STEPS
    Uds_logMin = math.log(Uds_min) / math.log(10)
    Uds_logMax = math.log(Uds_max) / math.log(10)
    Uds_step = (Uds_logMax - Uds_logMin) / (NUM_U_DS_STEPS - 1)

    if cgnd:
        scpi("INST:COUP:TRAC CGND")

    scpi("INST ch1")
    scpi("OUTP 0")
    scpi("VOLT 0")
    scpi("CURR " + str(Ig))
    scpi("OUTP:DPR 1")

    scpi("INST ch2")
    scpi("OUTP 0")
    scpi("VOLT 0")
    scpi("CURR " + str(Id_max))

    scpi('SENS:DLOG:TRAC:REM "Device name = ' + deviceName + '; Uds,max = ' + str(Uds_max) + '; Id,max = ' + str(Id_max) + '"')

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

            if TIME_OFF_MS > 0:
                setU(1, 0)
                t = ticks_add(t, TIME_OFF_MS)
                sleep_ms(ticks_diff(t, ticks_ms()))

        dlogTraceData(iMonValues)

ch1Model = scpi("SYSTem:CHANnel:MODel? ch1")
ch2Model = scpi("SYSTem:CHANnel:MODel? ch2")
if ch1Model.startswith("DCP405") and ch2Model.startswith("DCP405"):
    scpi("DISP:DIALog:OPEN \"/Scripts/Curve Tracer.res\"")

    updateData('Uce_max', 'VOLT', Uce_max)
    updateData('Ic_max', 'AMPER', Ic_max)
    updateData('Uds_max', 'VOLT', Uds_max)
    updateData('Id_max', 'AMPER', Id_max)

    while True:
        action = scpi("DISP:DIALog:ACTIon?")
        if action == "select_device_type":
            select_device_type()
        if action == "input_device_name":
            input_device_name()
        elif action == "input_Uce_max":
            input_Uce_max()
        elif action == "input_Ic_max":
            input_Ic_max()
        elif action == "input_Uds_max":
            input_Uds_max()
        elif action == "input_Id_max":
            input_Id_max()
        elif action == "start":
            start()
            break
        elif action == "close" or action == 0:
            scpi("DISP:DIALog:CLOSe")
            break
else:
    scpi('DISP:INPUT? "Requires DCP405 on Ch1 and Ch2", MENU, BUTTON, "Close"')

