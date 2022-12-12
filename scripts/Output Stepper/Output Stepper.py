# Adjusts output V and I in fixed steps
# Author: Mike G
# Version: 4.1

from eez import scpi

def input_voltage():
    global voltage, max_voltage, max_curr, max_pwr
    val = scpi('DISP:INPUT? "Set Output Voltage",NUMBER,VOLT,0.0,' + str(max_voltage) + '.0,' + str(voltage))
    if val != None:
        if float(set_curr) * float(val) > max_pwr:
            scpi('SYST:BEEP')
            scpi('DISP:INPUT? "Maximum power exceeded", MENU, BUTTON, "Close"')
        else:
            voltage = float(val)
            scpi("VOLT " + str(voltage))


def input_current():
    global set_curr,max_curr,voltage
    value = scpi('DISP:INPUT? "Set Current Limit",NUMBER,AMPE,0.0,' + str(max_curr) + '.0,' + str(set_curr))
    if value != None:
        if float(value) * float(voltage) > max_pwr:
            scpi('SYST:BEEP')
            scpi('DISP:INPUT? "Maximum power exceeded", MENU, BUTTON, "Close"')
        else: 
            set_curr = float(value)
            scpi("CURR " + str(value))
           


def adj(n):
    global voltage, max_voltage, volt,  max_pwr
    voltage = (voltage + n)
    if float(set_curr) * float(voltage) > max_pwr:
        voltage = (voltage - n)
        scpi('SYST:BEEP')
        scpi('DISP:INPUT? "Maximum power exceeded", MENU, BUTTON, "Close"')
    elif voltage > (max_voltage):
        voltage = (voltage - n)
        scpi('SYST:BEEP')
        scpi('DISP:INPUT? "Voltage above 40V", MENU, BUTTON, "Close"')
    else:
        scpi("VOLT " + str(voltage))
       
        
def adjc(n):
    global max_curr, set_curr, max_pwr,voltage
    set_curr = (set_curr + n )
    if float(set_curr) * float(voltage) > max_pwr:
        set_curr = (set_curr - n)
        scpi('SYST:BEEP')
        scpi('DISP:INPUT? "Maximum power exceeded", MENU, BUTTON, "Close"')
    elif set_curr > (max_curr):
        set_curr = (set_curr - n)
        scpi('SYST:BEEP')
        scpi('DISP:INPUT? "Current limit above 5.0 Amps", MENU, BUTTON, "Close"')
    else:
        scpi("CURR " + str(set_curr))
        

def meas_output():
    curr = scpi("MEAS:CURR?")
    scpi('DISP:DIAL:DATA "curr",FLOAT,AMPE,' + str(curr))
    vout = scpi("MEAS:VOLT?")
    scpi('DISP:DIAL:DATA "vout",FLOAT,VOLT,' + str(vout))


def main():
    global voltage, max_voltage, volt, n, set_curr,max_curr,max_pwr
    scpi("INST ch1")
    scpi("OUTP OFF")
    #voltage = scpi("VOLT?")
    max_voltage = scpi("VOLT? MAX")
    #value = scpi("VOLT?")
    voltage = 0.0
    scpi("VOLT " + str(voltage))
    max_curr = scpi("CURR? MAX")
    curr = scpi("MEAS:CURR?")
    #set_curri = scpi("CURR?")
    set_curr = 0.0
    scpi("CURR " + str(set_curr))
    max_pwr = scpi("POW:LIM?")
    scpi("DISP:DIAL:OPEN \"/Scripts/Output Stepper.res\"")
    
    try:
       
        while True:
            meas_output()
            scpi('DISP:DIAL:DATA "voltage",FLOAT,VOLT,' + str(voltage))
            scpi('DISP:DIAL:DATA "set_curr",FLOAT,AMPE,' + str(set_curr))
            action = scpi("DISP:DIALog:ACTIon? .1")
            if action == "input_voltage":
                input_voltage()
            elif action == "input_current":
                input_current()
            elif action == "off":
                scpi("OUTP OFF")
            elif action == "+1":
                adj(1)
            elif action == "+0.1":
                adj(.1)
            elif action == "+.01":
                adj(.01)
            elif action == "-1":
                adj(-1)
            elif action == "-0.1":
                adj(-.1)
            elif action == "-.01":
                adj(-.01)
            elif action == "+1c":
                adjc(1)
            elif action == "+0.1c":
                adjc(.1)
            elif action == "+.01c":
                adjc(.01)
            elif action == "-1c":
                adjc(-1)
            elif action == "-0.1c":
                adjc(-.1)
            elif action == "-.01c":
                adjc(-.01)
            elif action == "on":
                scpi("OUTP ON")
            elif action == "close" or action == 0:
                break
           
    finally:
        scpi("DISP:DIALog:CLOSe")
        

    
     
main()
