# Calculate parallel resistance/inductance or capacitance is series

from eez import scpi

TYPE_PARALLEL = 0
TYPE_SERIES = 1

circuitType = TYPE_PARALLEL

R1 = None
R2 = None
Rtot = None

C1 = None
C2 = None
Ctot = None

def inputResistance(name, resistance):
    value = scpi('DISP:INPUT? "' + name + '",NUMBER,OHM,-1E15,1E15,' + (str(resistance) if resistance != None else "1E3"))
    if value != None:
        resistance = float(value)
        scpi('DISP:DIALog:DATA "' + name + '",FLOAT,OHM,' + str(resistance))
    return resistance

def inputCapacitance(name, capacitance):
    value = scpi('DISP:INPUT? "' + name + '",NUMBER,FARAD,-1E15,1E15,' + (str(capacitance) if capacitance != None else "1E-6"))
    if value != None:
        capacitance = float(value)
        scpi('DISP:DIALog:DATA "' + name + '",FLOAT,FARAD,' + str(capacitance))
    return capacitance

def setResistanceData(name, value):
    scpi('DISP:DIALog:DATA "' + name + '",FLOAT,OHM,' + str(round(value, 4)))

def setCapacitanceData(name, value):
    scpi('DISP:DIALog:DATA "' + name + '",FLOAT,FARAD,' + str(round(value, 15)))

def error(message):
    scpi('DISP:ERROR "' + message + '"')

def select_type():
    global circuitType
    value = scpi('DISP:SELECT? ' + str(circuitType + 1) + ',"R or L in parallel","C in series"')
    if value != None and value != 0:
        circuitType = value - 1
        scpi("DISP:DIALog:DATA \"type\",INTEGER," + str(circuitType))

def input_R1():
    global R1
    R1 = inputResistance("R1", R1)

def input_R2():
    global R2
    R2 = inputResistance("R2", R2)

def input_Rtot():
    global Rtot
    Rtot = inputResistance("Rtot", Rtot)

def input_C1():
    global C1
    C1 = inputCapacitance("C1", C1)

def input_C2():
    global C2
    C2 = inputCapacitance("C2", C2)

def input_Ctot():
    global Ctot
    Ctot = inputCapacitance("Ctot", Ctot)

def can_calc_R1():
    return R2 != None and Rtot != None

def can_calc_R2():
    return R1 != None and Rtot != None

def can_calc_Rtot():
    return R1 != None and R2 != None

def can_calc_C1():
    return C2 != None and Ctot != None

def can_calc_C2():
    return C1 != None and Ctot != None

def can_calc_Ctot():
    return C1 != None and C2 != None

def calc_R1():
    if can_calc_R1():
        global R1
        R1 = R2 * Rtot / (R2 - Rtot)
        setResistanceData("R1", R1)
    else:
        scpi("DISP:ERROR \"Enter R2 and Rtot\"")

def calc_R2():
    if can_calc_R2():
        global R2
        R2 = R1 * Rtot / (R1 - Rtot)
        setResistanceData("R2", R2)
    else:
        scpi("DISP:ERROR \"Enter R1 and Rtot\"")

def calc_Rtot():
    if can_calc_Rtot():
        global Rtot
        Rtot = R1 * R2 / (R1 + R2)
        setResistanceData("Rtot", Rtot)
    else:
        scpi("DISP:ERROR \"Enter R1 and R2\"")

def calc_C1():
    if can_calc_C1():
        global C1
        C1 = C2 * Ctot / (C2 - Ctot)
        setCapacitanceData("C1", C1)
    else:
        scpi("DISP:ERROR \"Enter C2 and Ctot\"")

def calc_C2():
    if can_calc_C2():
        global C2
        C2 = C1 * Ctot / (C1 - Ctot)
        setCapacitanceData("C2", C2)
    else:
        scpi("DISP:ERROR \"Enter C1 and Ctot\"")

def calc_Ctot():
    if can_calc_Ctot():
        global Ctot
        Ctot = C1 * C2 / (C1 + C2)
        setCapacitanceData("Ctot", Ctot)
    else:
        scpi("DISP:ERROR \"Enter C1 and C2\"")

scpi("DISP:DIALog:OPEN \"/Scripts/Parallel and Series Calculator.res\"")
while True:
    action = scpi("DISP:DIALog:ACTIon?")
    if action == "select_type":
        select_type()
    elif action == "input_R1":
        input_R1()
    elif action == "input_R2":
        input_R2()
    elif action == "input_Rtot":
        input_Rtot()
    elif action == "input_C1":
        input_C1()
    elif action == "input_C2":
        input_C2()
    elif action == "input_Ctot":
        input_Ctot()
    elif action == "calc_R1":
        calc_R1()
    elif action == "calc_R2":
        calc_R2()
    elif action == "calc_Rtot":
        calc_Rtot()
    elif action == "calc_C1":
        calc_C1()
    elif action == "calc_C2":
        calc_C2()
    elif action == "calc_Ctot":
        calc_Ctot()
    elif action == "close" or action == 0:
        scpi("DISP:DIALog:CLOSe")
        break
