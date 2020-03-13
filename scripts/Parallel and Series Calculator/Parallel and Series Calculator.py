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

def select_type():
    global circuitType
    value = scpi('DISP:SELECT? ' + str(circuitType + 1) + ',"R or L in parallel","C in series"')
    if value != None and value != 0:
        circuitType = value - 1
        scpi("DISP:DIALog:DATA \"type\",INTEGER," + str(circuitType))

def input_R1():
    global R1
    value = scpi('DISP:INPUT? "R1",NUMBER,OHM,-1E15,1E15,' + (str(R1) if R1 != None else "1"))
    if value != None:
        R1 = float(value)
        scpi("DISP:DIALog:DATA \"R1\",FLOAT,OHM," + str(R1))

def input_R2():
    global R2
    value = scpi('DISP:INPUT? "R2",NUMBER,OHM,-1E15,1E15,' + (str(R2) if R2 != None else "1"))
    if value != None:
        R2 = float(value)
        scpi("DISP:DIALog:DATA \"R2\",FLOAT,OHM," + str(R2))

def input_Rtot():
    global Rtot
    value = scpi('DISP:INPUT? "Rtot",NUMBER,OHM,-1E15,1E15,' + (str(Rtot) if Rtot != None else "1"))
    if value != None:
        Rtot = float(value)
        scpi("DISP:DIALog:DATA \"Rtot\",FLOAT,OHM," + str(Rtot))

def input_C1():
    global C1
    value = scpi('DISP:INPUT? "C1",NUMBER,FARAD,-1E15,1E15,' + (str(C1) if C1 != None else "1E-6"))
    if value != None:
        C1 = float(value)
        scpi("DISP:DIALog:DATA \"C1\",FLOAT,FARAD," + str(C1))

def input_C2():
    global C2
    value = scpi('DISP:INPUT? "C2",NUMBER,FARAD,-1E15,1E15,' + (str(C2) if C2 != None else "1E-6"))
    if value != None:
        C2 = float(value)
        scpi("DISP:DIALog:DATA \"C2\",FLOAT,FARAD," + str(C2))

def input_Ctot():
    global Ctot
    value = scpi('DISP:INPUT? "Ctot",NUMBER,FARAD,-1E15,1E15,' + (str(Ctot) if Ctot != None else "1E-6"))
    if value != None:
        Ctot = float(value)
        scpi("DISP:DIALog:DATA \"Ctot\",FLOAT,FARAD," + str(Ctot))

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
        scpi("DISP:DIALog:DATA \"R1\",FLOAT,OHM," + str(round(R1, 3)))
    else:
        scpi("DISP:ERROR \"Enter R2 and Rtot\"")

def calc_R2():
    if can_calc_R2():
        global R2
        R2 = R1 * Rtot / (R1 - Rtot)
        scpi("DISP:DIALog:DATA \"R2\",FLOAT,OHM," + str(round(R2, 3)))
    else:
        scpi("DISP:ERROR \"Enter R1 and Rtot\"")

def calc_Rtot():
    if can_calc_Rtot():
        global Rtot
        Rtot = R1 * R2 / (R1 + R2)
        scpi("DISP:DIALog:DATA \"Rtot\",FLOAT,OHM," + str(round(Rtot, 3)))
    else:
        scpi("DISP:ERROR \"Enter R1 and R2\"")

def calc_C1():
    if can_calc_C1():
        global C1
        C1 = C2 * Ctot / (C2 - Ctot)
        scpi("DISP:DIALog:DATA \"C1\",FLOAT,FARAD," + str(round(C1, 15)))
    else:
        scpi("DISP:ERROR \"Enter C2 and Ctot\"")

def calc_C2():
    if can_calc_C2():
        global C2
        C2 = C1 * Ctot / (C1 - Ctot)
        scpi("DISP:DIALog:DATA \"C2\",FLOAT,FARAD," + str(round(C2, 15)))
    else:
        scpi("DISP:ERROR \"Enter C1 and Ctot\"")

def calc_Ctot():
    if can_calc_Ctot():
        global Ctot
        Ctot = C1 * C2 / (C1 + C2)
        scpi("DISP:DIALog:DATA \"Ctot\",FLOAT,FARAD," + str(round(Ctot, 15)))
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
