# Calculate resitor valus for voltage divider

from eez import scpi

Vin = None
Vout = None
R1 = None
R2 = None

def can_calc_Vout():
    return Vin != None and R1 != None and R2 != None

def calc_Vout():
    return Vin * R2 / (R1 + R2)

def can_calc_Vin():
    return Vout != None and R1 != None and R2 != None

def calc_Vin():
    return Vout * (R1 + R2) / R2

def can_calc_R1():
    return Vin != None and Vout != None and Vin > Vout and R2 != None

def calc_R1():
    return R2 * (Vin - Vout) / Vout

def can_calc_R2():
    return Vin != None and Vout != None and Vin > Vout and R1 != None

def calc_R2():
    return R1 * Vout / (Vin - Vout)

scpi("DEBUG 26, \"/Scripts/voltage-divider-calculator-background.jpg\"")
