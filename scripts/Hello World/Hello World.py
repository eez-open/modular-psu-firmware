# Hello, World!

from eez import scpi

def input_voltage():
    global voltage, max_voltage
    value = scpi('DISP:INPUT? "",NUMBER,VOLT,0.0,' + str(max_voltage) + '.0,' + str(voltage))
    if value != None:
        voltage = float(value)
        scpi('DISP:DIALog:DATA "voltage",FLOAT,VOLT,' + str(voltage))
        scpi('DISP:DIALog:DATA "can_set_voltage",INT,1')

def set_voltage():
    scpi("INST ch1")
    scpi("VOLT " + str(voltage))

def main():
    global voltage, max_voltage
    scpi("INST ch1")
    voltage = scpi("VOLT?")
    max_voltage = scpi("VOLT? MAX")

    scpi("DISP:DIAL:OPEN \"/Scripts/Hello World.res\"")
    try:
        scpi('DISP:DIAL:DATA "voltage",FLOAT,VOLT,' + str(voltage))

        while True:
            action = scpi("DISP:DIALog:ACTIon?")
            if action == "input_voltage":
                input_voltage()
            elif action == "set_voltage":
                set_voltage()
                break
            elif action == "close" or action == 0:
                break
    finally:
        scpi("DISP:DIAL:CLOS")

main()