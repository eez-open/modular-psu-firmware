# This calculator helps calculate values of resistive voltage divider

from eez import scpi

Vin = None
R1 = None
R2 = None
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

def input_Vin():
    global Vin
    Vin = inputVoltage("Vin", Vin)

def input_R1():
    global R1
    R1 = inputResistance("R1", R1)

def input_R2():
    global R2
    R2 = inputResistance("R2", R2)

def input_Vout():
    global Vout
    Vout = inputVoltage("Vout", Vout)

def can_calc_Vout():
    return Vin != None and R1 != None and R2 != None

def can_calc_R1():
    return Vin != None and Vout != None and Vin > Vout and R2 != None

def can_calc_R2():
    return Vin != None and Vout != None and Vin > Vout and R1 != None

def can_calc_Vin():
    return Vout != None and R1 != None and R2 != None

def calc_Vin():
    if can_calc_Vin():
        global Vin
        Vin = Vout * (R1 + R2) / R2
        setVoltageData("Vin", Vin)
    else:
        error("Enter R1, R2 and Vout")

def calc_R1():
    if can_calc_R1():
        global R1
        R1 = R2 * (Vin - Vout) / Vout
        setResistanceData("R1", R1)
    else:
        if Vin != None and Vout != None and R2 != None:
            error("Vout must be less then Vin")
        else:
            error("Enter Vin, R2 and Vout")

def calc_R2():
    if can_calc_R2():
        global R2
        R2 = R1 * Vout / (Vin - Vout)
        setResistanceData("R2", R2)
    else:
        if Vin != None and Vout != None and R1 != None:
            error("Vout must be less then Vin")
        else:
            error("Enter Vin, R1 and Vout")

def calc_Vout():
    if can_calc_Vout():
        global Vout
        Vout = Vin * R2 / (R1 + R2)
        setVoltageData("Vout", Vout)
    else:
        error("Enter Vin, R1 and R2")

scpi("DISP:DIALog:OPEN \"/Scripts/Voltage Divider Calculator.res\"")
while True:
    action = scpi("DISP:DIALog:ACTIon?")
    if action == "input_Vin":
        input_Vin()
    elif action == "input_R1":
        input_R1()
    elif action == "input_R2":
        input_R2()
    elif action == "input_Vout":
        input_Vout()
    elif action == "calc_Vin":
        calc_Vin()
    elif action == "calc_R1":
        calc_R1()
    elif action == "calc_R2":
        calc_R2()
    elif action == "calc_Vout":
        calc_Vout()
    elif action == "close" or action == 0:
        scpi("DISP:DIALog:CLOSe")
        break

