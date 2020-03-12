# This calculator helps calculate values of resistive voltage divider

from eez import scpi

Vin = None
R1 = None
R2 = None
Vout = None

def input_Vin():
    global Vin
    value = scpi('DISP:INPUT? "Vin", NUMBER,VOLT,1E-3,1E3,' + (str(Vin) if Vin != None else "1"))
    if value != None:
        Vin = float(value)
        scpi("DISP:DIALog:DATA \"Vin\",FLOAT,VOLT," + str(Vin))

def input_R1():
    global R1
    value = scpi('DISP:INPUT? "R1",NUMBER,OHM,1E-12,1E12,' + (str(R1) if R1 != None else "1"))
    if value != None:
        R1 = float(value)
        scpi("DISP:DIALog:DATA \"R1\",FLOAT,OHM," + str(R1))

def input_R2():
    global R2
    value = scpi('DISP:INPUT? "R2",NUMBER,OHM,1E-12,1E12,' + (str(R2) if R2 != None else "1"))
    if value != None:
        R2 = float(value)
        scpi("DISP:DIALog:DATA \"R2\",FLOAT,OHM," + str(R2))

def input_Vout():
    global Vout
    value = scpi('DISP:INPUT? "Vout",NUMBER,VOLT,1E-3,1E3,' + (str(Vout) if Vout != None else "1"))
    if value != None:
        Vout = float(value)
        scpi("DISP:DIALog:DATA \"Vout\",FLOAT,VOLT," + str(Vout))

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
        scpi("DISP:DIALog:DATA \"Vin\",FLOAT,VOLT," + str(round(Vin, 3)))
    else:
        scpi("DISP:ERROR \"Enter R1, R2 and Vout\"")

def calc_R1():
    if can_calc_R1():
        global R1
        R1 = R2 * (Vin - Vout) / Vout
        scpi("DISP:DIALog:DATA \"R1\",FLOAT,OHM," + str(round(R1, 3)))
    else:
        if Vin != None and Vout != None and R2 != None:
            scpi("DISP:ERROR \"Vout must be less then Vin\"")
        else:
            scpi("DISP:ERROR \"Enter Vin, R2 and Vout\"")

def calc_R2():
    if can_calc_R2():
        global R2
        R2 = R1 * Vout / (Vin - Vout)
        scpi("DISP:DIALog:DATA \"R2\",FLOAT,OHM," + str(round(R2, 3)))
    else:
        if Vin != None and Vout != None and R1 != None:
            scpi("DISP:ERROR \"Vout must be less then Vin\"")
        else:
            scpi("DISP:ERROR \"Enter Vin, R1 and Vout\"")

def calc_Vout():
    if can_calc_Vout():
        global Vout
        Vout = Vin * R2 / (R1 + R2)
        scpi("DISP:DIALog:DATA \"Vout\",FLOAT,VOLT," + str(round(Vout, 3)))
    else:
        scpi("DISP:ERROR \"Enter Vin, R1 and R2\"")

scpi("DISP:DIALog:OPEN \"/Scripts/voltage-divider-calculator.res\"")
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

