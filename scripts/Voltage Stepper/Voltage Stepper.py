# Allows 7 preset V and I for channels 1 & 2
# Author: Mike G
# Version: 4.8 

from eez import scpi


def adj(n):
    global voltage, max_voltage, volt, max_pwr, VOLTS, set_curr

    val = scpi('DISP:INPUT? "Set Step ' + str(n + 1) + ' Voltage",NUMBER,VOLT, 0.0, 40.0,' + str(VOLTS[c][n]))
    if val != None:
        if float(val) * float(ISET[c][n]) > max_pwr:
            volt_max = max_pwr/(ISET[c][n])
            scpi('SYST:BEEP')
            scpi('DISP:INPUT? "Max volts on Step ' + str(n + 1) + ' - ' + str(volt_max) +  '", MENU, BUTTON, "Close"')
        else:
            VOLTS[c][n] = float(val)
            scpi('DISP:DIALog:DATA "v' + str(n + 1) +'",FLOAT,VOLT,' + str(VOLTS[c][n]))
            scpi("VOLT " + str(VOLTS[c][pos[c]]))
            

def input_volt():
    global set_curr, max_curr, voltage, max_pwr, VOLTS, pos, n
    
    value = scpi('DISP:INPUT? "Set Global Voltage",NUMBER,VOLT, 0.0, 40.0,' + str(VOLTS[c][pos[c]]))
    
    if value != None:
        for x in range(7):
            if float(value) * float(ISET[c][x]) > max_pwr:
                curr_max = round((max_pwr)/(value),2)
                (ISET[c][x]) = (curr_max)
                (VOLTS[c][x]) = float(value)
                scpi('DISP:DIALog:DATA "i' + str(x + 1) + '",FLOAT,AMPE,' + str(ISET[c][x]))
                scpi('DISP:DIALog:DATA "v' + str(x + 1) + '",FLOAT,VOLT,' + str(VOLTS[c][x]))
                scpi("CURR " + str(ISET[c][pos[c]]))
                scpi("VOLT " + str(VOLTS[c][pos[c]]))
                scpi('SYST:BEEP')
                scpi('DISP:INPUT? "Current on Step ' + str(x + 1) + ' reduced to  ' + str(curr_max) +  'A", MENU, BUTTON, "Close"')
            else:    
                (VOLTS[c][x]) = float(value) 
                scpi('DISP:DIALog:DATA "v' + str(x + 1) + '",FLOAT,VOLT,' + str(VOLTS[c][x]))
                scpi("VOLT " + str(VOLTS[c][pos[c]])) 
    
    
def adjI(n):
    global set_curr, max_curr, voltage, max_pwr
    value = scpi('DISP:INPUT? "Set Step ' + str(n + 1) +' Curr Lim",NUMBER,AMPE, 0.0, 5.0,' + str(ISET[c][n]))
    if value != None:
        if float(value) * float(VOLTS[c][n]) > max_pwr:
            curr_max = max_pwr/(VOLTS[c][n])
            scpi('SYST:BEEP')
            scpi('DISP:INPUT? "Max current on Step ' + str(n + 1) + ' - ' + str(curr_max) +  '", MENU, BUTTON, "Close"')
        else:    
            ISET[c][n] = float(value)
            scpi('DISP:DIALog:DATA "i' + str(n + 1) + '",FLOAT,AMPE,' + str(ISET[c][n]))
            scpi("CURR " + str(ISET[c][pos[c]]))

def input_current():
    global  max_curr, voltage, max_pwr,set_curr
    value = scpi('DISP:INPUT? "Set Global Curr Lim",NUMBER,AMPE, 0.0, 5.0,' + str(set_curr))
    if value != None:
        for x in range(7):
            if float(value) * float(VOLTS[c][x]) > max_pwr:
                volt_max = round((max_pwr)/(value))
                (VOLTS[c][x]) = (volt_max)
                scpi('DISP:DIALog:DATA "v' + str(x + 1) + '",FLOAT,VOLT,' + str(VOLTS[c][x]))
                (ISET[c][x]) = float(value)
                scpi('DISP:DIALog:DATA "i' + str(x + 1) + '",FLOAT,AMPE,' + str(ISET[c][x]))
                scpi("VOLT " + str(VOLTS[c][pos[c]]))
                scpi("CURR " + str(ISET[c][pos[c]]))
                scpi('SYST:BEEP')
                scpi('DISP:INPUT? "Volts on Step ' + str(x + 1) + ' set to  ' + str(VOLTS[c][x]) + '.0V", MENU, BUTTON,"Close"')
            else:    
                (ISET[c][x]) = float(value)
                scpi('DISP:DIALog:DATA "i' + str(x + 1) + '",FLOAT,AMPE,' + str(ISET[c][x]))
                scpi("CURR " + str(ISET[c][pos[c]]))
    
            
def meas_output():
    curr = scpi("MEAS:CURR?")
    scpi('DISP:DIAL:DATA "curr",FLOAT,AMPE,' + str(curr))
    vout = scpi("MEAS:VOLT?")
    scpi('DISP:DIAL:DATA "vout",FLOAT,VOLT,' + str(vout))
    
    
def ch(s):
    global c, set_curr, max_voltage, max_pwr, max_curr, voltage 
    c = s
    if c == 1:
        scpi("INST ch2")
    else:
        scpi("INST ch1")

    max_voltage = scpi("VOLT? MAX")
    max_pwr = scpi("POW:LIM?") -10
    max_curr = scpi("CURR? MAX")
    
    for x in range(7):
        scpi('DISP:DIALog:DATA "i' + str(x + 1) + '",FLOAT,AMPE,' + str(ISET[c][x]))
        scpi('DISP:DIALog:DATA "v' + str(x + 1) + '",FLOAT,VOLT,' + str(VOLTS[c][x]))
    
    
def right():
    global pos
    if pos[c]  == len(VOLTS[0]) - 1:
	    pos[c] = 0
    else:
	    pos[c] = pos[c] + 1


def left():
    global pos
    if pos[c]  ==  0:
        pos[c] = len(VOLTS[0]) - 1
    else:
	    pos[c] = pos[c] - 1
	    
def newp(p):
    global pos
    pos[c] = (p-1)


def write_settings():
    global VOLTS, ISET, pos
    
    SETTINGS_FILE_PATH = "/Scripts/volt_step.settings48"
    # convert VOLTS, ISET and pos to list of strings and store into settings
    settings = []
    for ch_index in range(2):
        for volt_index in range(7):
            settings.append(str(VOLTS[ch_index][volt_index]))
        for iset_index in range(7):
            settings.append(str(ISET[ch_index][iset_index]))
        settings.append(str(pos[ch_index]))
    
    settings_str = "\n".join(settings)
    settings_str_len = str(len(settings_str))

    scpi('MMEM:DOWN:FNAM "' + SETTINGS_FILE_PATH + '"')
    scpi('MMEM:DOWN:SIZE ' + settings_str_len)
    scpi('MMEM:DOWN:DATA #' + str(len(settings_str_len)) + settings_str_len + settings_str)
    scpi('MMEM:DOWN:FNAM ""')
    
def file_exists(file_path):
    i = file_path.rfind("/")
    folder = file_path[:i]
    file_name = file_path[i+1:]
    cat = scpi('MMEM:CAT? "' + folder + '"')
    return file_name in cat
    
def read_settings():
    global VOLTS, ISET, pos
    
    SETTINGS_FILE_PATH = "/Scripts/volt_step.settings48"
    
    if not file_exists(SETTINGS_FILE_PATH):
        return False
    
    try:
        settings_str = scpi('MMEM:UPL? "' + SETTINGS_FILE_PATH + '"')
        skip = 2 + int(settings_str[1])
        settings_str = settings_str[skip:]
        settings = settings_str.split("\n")
        # convert settings (list of strings) into VOLTS, ISET and pos
        settings_index = 0
        VOLTS = []
        ISET = []
        pos = []
        for ch_index in range(2):
            VOLTS.append([])
            for volt_index in range(7):
                VOLTS[ch_index].append(float(settings[settings_index]))
                settings_index += 1
            ISET.append([])
            for iset_index in range(7):
                ISET[ch_index].append(float(settings[settings_index]))
                settings_index += 1
            pos.append(int(settings[settings_index]))
            settings_index += 1
        return True;
    except:
        return False


def main():
    global voltage, max_voltage, volt, n, max_pwr, VOLTS, pos, set_curr, curr, ISET, set_volt, c
    
    if not read_settings():
        # set to defaults
        VOLTS = [[1.1, 2.2, 3.3, 4.4, 5.5, 6.6, 7.7], [11, 12, 13, 14, 15, 16, 17]]
        ISET  = [[.1, .2, .3, .4, .5, .6, .7], [.11, .22, .33, .44, .55, .66, .77]]
        pos = [0, 0]

    scpi("INST ch2")
    scpi("OUTP OFF")
    scpi("INST ch1")
    scpi("OUTP OFF")

    scpi("DISP:DIAL:OPEN \"/Scripts/Voltage Stepper.res\"")
    ch(0)
    
    try:
        while True:
            meas_output()
            scpi("DISP:DIALog:DATA \"position\",INTEGER," + str(pos[c]))
            scpi("VOLT " + str(VOLTS[c][pos[c]]))
            scpi("CURR " + str(ISET[c][pos[c]]))
            set_curr = scpi("CURR?")
            scpi('DISP:DIALog:DATA "set_curr",FLOAT,AMPE,' + str(set_curr))
            voltage = scpi("VOLT?")
            scpi('DISP:DIALog:DATA "voltage",FLOAT,VOLT,' + str(voltage))
            scpi('DISP:DIAL:DATA "chann",FLOAT,UNKN,' + str(c+1))
            
            action = scpi("DISP:DIALog:ACTIon? .1")
            if action == "input_volt":
                input_volt()
            elif action == "input_current":
                input_current()
            elif action == "input_v1":
                adj(0)
            elif action == "input_i1":
                adjI(0)
            elif action == "input_v2":
                adj(1)
            elif action == "input_i2":
                adjI(1)
            elif action == "input_v3":
                adj(2)
            elif action == "input_i3":
                adjI(2)
            elif action == "input_v4":
                adj(3)
            elif action == "input_i4":
                adjI(3)
            elif action == "input_v5":
                adj(4)
            elif action == "input_i5":
                adjI(4)
            elif action == "input_v6":
                adj(5)
            elif action == "input_i6":
                adjI(5)
            elif action == "input_v7":
                adj(6)
            elif action == "input_i7":
                adjI(6)
            elif action == "p1":
                newp(1)
            elif action == "p2":
                newp(2)
            elif action == "p3":
                newp(3)
            elif action == "p4":
                newp(4)
            elif action == "p5":
                newp(5)
            elif action == "p6":
                newp(6)
            elif action == "p7":
                newp(7)
            elif action == "step_r":
                right()
            elif action == "step_l":
                left()
            elif action == "sel_ch1":
                ch(0)
            elif action == "sel_ch2":
                ch(1)
            elif action == "on":
                scpi("OUTP ON")
            elif action == "off":
                scpi("OUTP OFF")
            elif action == "close" or action == 0:
                break
      
    finally:
        scpi("DISP:DIALog:CLOSe")
        write_settings()
    
main()