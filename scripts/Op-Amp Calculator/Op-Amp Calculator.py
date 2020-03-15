# This calculator helps calculate values of the op-amp configured as inverting, non-inverting or differential amplifier

from eez import scpi

TYPE_NON_INV = 0
TYPE_INV = 1
TYPE_DIFF_SIMPL = 2
TYPE_DIFF = 3

opAmpType = TYPE_NON_INV

Vin = None
Vin1 = None
Vin2 = None

R1 = None
R2 = None
Rf = None
Rg = None

Vout = None

def select_type():
    global opAmpType
    value = scpi('DISP:SELECT? ' + str(opAmpType + 1) + ',"Non-inverting","Inverting","Diff. simplified","Differential"')
    if value != None and value != 0:
        opAmpType = value - 1
        scpi("DISP:DIALog:DATA \"type\",INTEGER," + str(opAmpType))

def input_Vin():
    global Vin
    value = scpi('DISP:INPUT? "Vin",NUMBER,VOLT,-1E3,1E3,' + (str(Vin) if Vin != None else "1"))
    if value != None:
        Vin = float(value)
        scpi("DISP:DIALog:DATA \"Vin\",FLOAT,VOLT," + str(Vin))

def input_Vin1():
    global Vin1
    value = scpi('DISP:INPUT? "Vin1",NUMBER,VOLT,-1E3,1E3,' + (str(Vin1) if Vin1 != None else "1"))
    if value != None:
        Vin1 = float(value)
        scpi("DISP:DIALog:DATA \"Vin1\",FLOAT,VOLT," + str(Vin1))

def input_Vin2():
    global Vin2
    value = scpi('DISP:INPUT? "Vin2",NUMBER,VOLT,-1E3,1E3,' + (str(Vin2) if Vin2 != None else "1"))
    if value != None:
        Vin2 = float(value)
        scpi("DISP:DIALog:DATA \"Vin2\",FLOAT,VOLT," + str(Vin2))

def input_R1():
    global R1
    value = scpi('DISP:INPUT? "R1",NUMBER,OHM,-1E12,1E12,' + (str(R1) if R1 != None else "1"))
    if value != None:
        R1 = float(value)
        scpi("DISP:DIALog:DATA \"R1\",FLOAT,OHM," + str(R1))

def input_R2():
    global R2
    value = scpi('DISP:INPUT? "R2",NUMBER,OHM,-1E12,1E12,' + (str(R2) if R2 != None else "1"))
    if value != None:
        R2 = float(value)
        scpi("DISP:DIALog:DATA \"R2\",FLOAT,OHM," + str(R2))

def input_Rf():
    global Rf
    value = scpi('DISP:INPUT? "Rf",NUMBER,OHM,-1E12,1E12,' + (str(Rf) if Rf != None else "1"))
    if value != None:
        Rf = float(value)
        scpi("DISP:DIALog:DATA \"Rf\",FLOAT,OHM," + str(Rf))

def input_Rg():
    global Rg
    value = scpi('DISP:INPUT? "Rg",NUMBER,OHM,-1E12,1E12,' + (str(Rg) if Rg != None else "1"))
    if value != None:
        Rg = float(value)
        scpi("DISP:DIALog:DATA \"Rg\",FLOAT,OHM," + str(Rg))
        
def input_Vout():
    global Vout
    value = scpi('DISP:INPUT? "Vout",NUMBER,VOLT,-1E3,1E3,' + (str(Vout) if Vout != None else "1"))
    if value != None:
        Vout = float(value)
        scpi("DISP:DIALog:DATA \"Vout\",FLOAT,VOLT," + str(Vout))

def can_calc_Vout():
    if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
        return Vin != None and R1 != None and R2 != None
    elif opAmpType == TYPE_DIFF_SIMPL:
        return Vin1 != None and Vin2 != None and R1 != None and R2 != None
    else:
        return Vin1 != None and Vin2 != None and R1 != None and R2 != None and Rf != None and Rg != None

def can_calc_Vin1():
    if opAmpType == TYPE_DIFF_SIMPL:
        return Vin2 != None and R1 != None and R2 != None and Vout != None
    elif opAmpType == TYPE_DIFF:
        return Vin2 != None and R1 != None and R2 != None and Rf != None and Rg != None and Vout != None
    else:
        return False

def can_calc_Vin2():
    if opAmpType == TYPE_DIFF_SIMPL:
        return Vin1 != None and R1 != None and R2 != None and Vout != None
    elif opAmpType == TYPE_DIFF:
        return Vin1 != None and R1 != None and R2 != None and Rf != None and Rg != None and Vout != None
    else:
        return False

def can_calc_R1():
    if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
        return Vin != None and R2 != None and Vout != None
    elif opAmpType == TYPE_DIFF_SIMPL:
        return Vin1 != None and Vin2 != None and R2 != None and Vout != None
    else:
        return Vin1 != None and Vin2 != None and R2 != None and Rf != None and Rg != None and Vout != None

def can_calc_R2():
    if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
        return Vin != None and R1 != None and Vout != None
    elif opAmpType == TYPE_DIFF_SIMPL:
        return Vin1 != None and Vin2 != None and R1 != None and Vout != None
    else:
        return Vin1 != None and Vin2 != None and R1 != None and Rf != None and Rg != None and Vout != None

def can_calc_Rf():
    if opAmpType == TYPE_DIFF:
        return Vin1 != None and Vin2 != None and R1 != None and R2 != None and Rg != None and Vout != None
    else:
        return False

def can_calc_Rg():
    if opAmpType == TYPE_DIFF:
        return Vin1 != None and Vin2 != None and R1 != None and R2 != None and Rf != None and Vout != None
    else:
        return False

def can_calc_Vin():
    if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
        return R1 != None and R2 != None and Vout != None
    else:
        return False

def calc_Vin():
    if can_calc_Vin():
        global Vin
        if opAmpType == TYPE_NON_INV:
            Vin = Vout * R2 / (R1 + R2)
        else:
            Vin = -Vout * R1 / R2
        scpi("DISP:DIALog:DATA \"Vin\",FLOAT,VOLT," + str(round(Vin, 3)))
    else:
        scpi("DISP:ERROR \"Enter R1, R2 and Vout\"")

def calc_Vin1():
    if can_calc_Vin1():
        global Vin1
        if opAmpType == TYPE_DIFF_SIMPL:
            Vin1 = Vout * R1 / R2 + Vin2
        else:
            Vin1 = (Vout + Vin2 * Rf / R2) * ((R1 + Rg) * R2) / (Rg * (R2 + Rf))
        scpi("DISP:DIALog:DATA \"Vin1\",FLOAT,VOLT," + str(round(Vin1, 3)))
    else:
        if opAmpType == TYPE_DIFF_SIMPL:
            scpi("DISP:ERROR \"Enter Vin2, R1, R2 and Vout\"")
        else:
            scpi("DISP:ERROR \"Enter Vin2, R1, R2, Rf, Rg and Vout\"")

def calc_Vin2():
    if can_calc_Vin2():
        global Vin2
        if opAmpType == TYPE_DIFF_SIMPL:
            Vin2 = Vin1 - Vout * R1 / R2
        else:
            Vin2 = (Vin1 * Rg / (R1 + Rg) * (R2 + Rf) / R2 - Vout) * R2 / Rf
        scpi("DISP:DIALog:DATA \"Vin2\",FLOAT,VOLT," + str(round(Vin2, 3)))
    else:
        if opAmpType == TYPE_DIFF_SIMPL:
            scpi("DISP:ERROR \"Enter Vin1, R1, R2 and Vout\"")
        else:
            scpi("DISP:ERROR \"Enter Vin2, R1, R2, Rf, Rg and Vout\"")

def calc_R1():
    if can_calc_R1():
        global R1
        if opAmpType == TYPE_NON_INV:
            R1 = R2 * (Vout - Vin) / Vin
        elif opAmpType == TYPE_INV:
            R1 = -R2 * Vin / Vout
        elif opAmpType == TYPE_DIFF_SIMPL:
            R1 = R2 * (Vin1 - Vin2) / Vout
        else:
            R1 = (Vin1 * (R2 + Rf) * Rg) / (Vout * R2 + Vin2 * Rf) - Rg
        scpi("DISP:DIALog:DATA \"R1\",FLOAT,OHM," + str(round(R1, 3)))
    else:
        if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
            scpi("DISP:ERROR \"Enter Vin, R2 and Vout\"")
        elif opAmpType == TYPE_DIFF_SIMPL:
            scpi("DISP:ERROR \"Enter Vin1, Vin2, R2 and Vout\"")
        else:
            scpi("DISP:ERROR \"Enter Vin1, Vin2, R2, Rf, Rg and Vout\"")

def calc_R2():
    if can_calc_R2():
        global R1
        if opAmpType == TYPE_NON_INV:
            R2 = R1 * Vin / (Vout - Vin)
        elif opAmpType == TYPE_INV:
            R2 = -R1 * Vout / Vin
        elif opAmpType == TYPE_DIFF_SIMPL:
            R2 = R1 * Vout / (Vin1 - Vin2)
        else:
            R2 = Rf * (Vin1 * Rg - Vin2 * (R1 + Rg)) / (Vout * (R1 + Rg) - Vin1 * Rg)
        scpi("DISP:DIALog:DATA \"R2\",FLOAT,OHM," + str(round(R2, 3)))
    else:
        if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
            scpi("DISP:ERROR \"Enter Vin, R1 and Vout\"")
        elif opAmpType == TYPE_DIFF_SIMPL:
            scpi("DISP:ERROR \"Enter Vin1, Vin2, R1 and Vout\"")
        else:
            scpi("DISP:ERROR \"Enter Vin1, Vin2, R1, Rf, Rg and Vout\"")

def calc_Rf():
    if can_calc_Rf():
        global Rf
        Rf = (Vout * R2 * (R1 + Rg) - Vin1 * R2 * Rg) / (Vin1 * Rg - Vin2 * (R1 + Rg))
        scpi("DISP:DIALog:DATA \"Rf\",FLOAT,OHM," + str(round(Rf, 3)))
    else:
        scpi("DISP:ERROR \"Enter Vin1, Vin2, R1, R2, Rg and Vout\"")

def calc_Rg():
    if can_calc_Rg():
        global Rg
        x = (Vout * R2 + Vin2 * Rf) / (Vin1 * (R2 + Rf))
        Rg = R1 * x / (1 - x)
        scpi("DISP:DIALog:DATA \"Rg\",FLOAT,OHM," + str(round(Rg, 3)))
    else:
        scpi("DISP:ERROR \"Enter Vin1, Vin2, R1, R2, Rf and Vout\"")

def calc_Vout():
    if can_calc_Vout():
        global Vout
        if opAmpType == TYPE_NON_INV:
            Vout = Vin * (1 + R1 / R2)
        elif opAmpType == TYPE_INV:
            Vout = -Vin * R2 / R1
        elif opAmpType == TYPE_DIFF_SIMPL:
            Vout = R2 / R1 * (Vin1 - Vin2)
        else:
            Vout = -Vin2 * Rf / R2 + Vin1 * Rg / (R1 + Rg) * (R2 + Rf) / R2
        scpi("DISP:DIALog:DATA \"Vout\",FLOAT,VOLT," + str(round(Vout, 3)))
    else:
        if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
            scpi("DISP:ERROR \"Enter Vin, R1 and R2\"")
        elif opAmpType == TYPE_DIFF_SIMPL:
            scpi("DISP:ERROR \"Enter Vin1, Vin2, R1 and R2\"")
        else:
            scpi("DISP:ERROR \"Enter Vin1, Vin2, R1, R2, Rf and Rg\"")

scpi("DISP:DIALog:OPEN \"/Scripts/Op-Amp Calculator.res\"")
while True:
    action = scpi("DISP:DIALog:ACTIon?")
    if action == "select_type":
        select_type()
    if action == "input_Vin":
        input_Vin()
    elif action == "input_Vin1":
        input_Vin1()
    elif action == "input_Vin2":
        input_Vin2()
    elif action == "input_R1":
        input_R1()
    elif action == "input_R2":
        input_R2()
    elif action == "input_Rf":
        input_Rf()
    elif action == "input_Rg":
        input_Rg()
    elif action == "input_Vout":
        input_Vout()
    elif action == "calc_Vin":
        calc_Vin()
    elif action == "calc_Vin1":
        calc_Vin1()
    elif action == "calc_Vin2":
        calc_Vin2()
    elif action == "calc_R1":
        calc_R1()
    elif action == "calc_R2":
        calc_R2()
    elif action == "calc_Rf":
        calc_Rf()
    elif action == "calc_Rg":
        calc_Rg()
    elif action == "calc_Vout":
        calc_Vout()
    elif action == "close" or action == 0:
        scpi("DISP:DIALog:CLOSe")
        break

