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

def inputVoltage(name, voltage):
    value = scpi('DISP:INPUT? "' + name + '",NUMBER,VOLT,-1E6,1E6,' + (str(voltage) if voltage != None else "1"))
    if value != None:
        voltage = float(value)
        scpi('DISP:DIALog:DATA "' + name + '",FLOAT,VOLT,' + str(voltage))
    return voltage

def inputResistance(name, resistance):
    value = scpi('DISP:INPUT? "' + name + '",NUMBER,OHM,-1E15,1E15,' + (str(resistance) if resistance != None else "1E3"))
    if value != None:
        resistance = float(value)
        scpi('DISP:DIALog:DATA "' + name + '",FLOAT,OHM,' + str(resistance))
    return resistance

def setVoltageData(name, value):
    scpi('DISP:DIALog:DATA "' + name + '",FLOAT,VOLT,' + str(round(value, 4)))

def setResistanceData(name, value):
    scpi('DISP:DIALog:DATA "' + name + '",FLOAT,OHM,' + str(round(value, 4)))

def error(message):
    scpi('DISP:ERROR "' + message + '"')

def select_type():
    global opAmpType
    value = scpi('DISP:SELECT? ' + str(opAmpType + 1) + ',"Non-inverting","Inverting","Diff. simplified","Differential"')
    if value != None and value != 0:
        opAmpType = value - 1
        scpi("DISP:DIALog:DATA \"type\",INTEGER," + str(opAmpType))

def input_Vin():
    global Vin
    Vin = inputVoltage("Vin", Vin)

def input_Vin1():
    global Vin1
    Vin1 = inputVoltage("Vin1", Vin1)

def input_Vin2():
    global Vin2
    Vin2 = inputVoltage("Vin2", Vin2)

def input_R1():
    global R1
    R1 = inputResistance("R1", R1)

def input_R2():
    global R2
    R2 = inputResistance("R2", R2)

def input_Rf():
    global Rf
    Rf = inputResistance("Rf", Rf)

def input_Rg():
    global Rg
    Rg = inputResistance("Rg", Rg)
        
def input_Vout():
    global Vout
    Vout = inputVoltage("Vout", Vout)

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
        setVoltageData("Vin", Vin)
    else:
        error("Enter R1, R2 and Vout")

def calc_Vin1():
    if can_calc_Vin1():
        global Vin1
        if opAmpType == TYPE_DIFF_SIMPL:
            Vin1 = Vout * R1 / R2 + Vin2
        else:
            Vin1 = (Vout + Vin2 * Rf / R2) * ((R1 + Rg) * R2) / (Rg * (R2 + Rf))
        setVoltageData("Vin1", Vin1)
    else:
        if opAmpType == TYPE_DIFF_SIMPL:
            error("Enter Vin2, R1, R2 and Vout")
        else:
            error("Enter Vin2, R1, R2, Rf, Rg and Vout")

def calc_Vin2():
    if can_calc_Vin2():
        global Vin2
        if opAmpType == TYPE_DIFF_SIMPL:
            Vin2 = Vin1 - Vout * R1 / R2
        else:
            Vin2 = ((Vin1 * Rg * (R2 + Rf) / (R1 + Rg)) - Vout * R2) / Rf
        setVoltageData("Vin2", Vin2)
    else:
        if opAmpType == TYPE_DIFF_SIMPL:
            error("Enter Vin1, R1, R2 and Vout")
        else:
            error("Enter Vin2, R1, R2, Rf, Rg and Vout")

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
        setResistanceData("R1", R1)
    else:
        if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
            error("Enter Vin, R2 and Vout")
        elif opAmpType == TYPE_DIFF_SIMPL:
            error("Enter Vin1, Vin2, R2 and Vout")
        else:
            error("Enter Vin1, Vin2, R2, Rf, Rg and Vout")

def calc_R2():
    if can_calc_R2():
        global R2
        if opAmpType == TYPE_NON_INV:
            R2 = R1 * Vin / (Vout - Vin)
        elif opAmpType == TYPE_INV:
            R2 = -R1 * Vout / Vin
        elif opAmpType == TYPE_DIFF_SIMPL:
            R2 = R1 * Vout / (Vin1 - Vin2)
        else:
            R2 = Rf * (Vin1 * Rg - Vin2 * (R1 + Rg)) / (Vout * (R1 + Rg) - Vin1 * Rg)
        setResistanceData("R2", R2)
    else:
        if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
            error("Enter Vin, R1 and Vout")
        elif opAmpType == TYPE_DIFF_SIMPL:
            error("Enter Vin1, Vin2, R1 and Vout")
        else:
            error("Enter Vin1, Vin2, R1, Rf, Rg and Vout")

def calc_Rf():
    if can_calc_Rf():
        global Rf
        Rf = (Vout * R2 * (R1 + Rg) - Vin1 * R2 * Rg) / (Vin1 * Rg - Vin2 * (R1 + Rg))
        setResistanceData("Rf", Rf)
    else:
        error("Enter Vin1, Vin2, R1, R2, Rg and Vout")

def calc_Rg():
    if can_calc_Rg():
        global Rg
        x = (Vout * R2 + Vin2 * Rf) / (Vin1 * (R2 + Rf))
        Rg = R1 * x / (1 - x)
        setResistanceData("Rg", Rg)
    else:
        error("Enter Vin1, Vin2, R1, R2, Rf and Vout")

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
        setVoltageData("Vout", Vout)
    else:
        if opAmpType == TYPE_NON_INV or opAmpType == TYPE_INV:
            error("Enter Vin, R1 and R2")
        elif opAmpType == TYPE_DIFF_SIMPL:
            error("Enter Vin1, Vin2, R1 and R2")
        else:
            error("Enter Vin1, Vin2, R1, R2, Rf and Rg")

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

