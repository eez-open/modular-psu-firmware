# Reform Electrolytic Capacitors with a specific profile for it

from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez   import scpi, setU, setI, getI, getU, dlogTraceData

#Default Values
cap_max_volt = 25
charge_current = 0.0001
reform_time = 20

channel = 1    # Not changable

module_max_volt = 0
have_data = False

TIME_STEP = 0.05  # Measure interval in secods. Increase to reduce log size

STEP1 = 0.25
STEP2 = 0.60
STEP3 = 1.00

def input_cap_max_volt():
  global cap_max_volt
  value = scpi('DISP:INPUT? "Max Cap Voltage",NUMBER,VOLT,1.0,' +
            str(module_max_volt) + ',' +
            str(cap_max_volt))
  if value != None:
      cap_max_volt = float(value)
      scpi('DISP:DIALog:DATA "cap_max_volt", FLOAT, VOLT,' +
        str(cap_max_volt))

def input_charge_current():
  global charge_current
  value = scpi('DISP:INPUT? "Charge current",NUMBER,AMPER,0.0,80.0,'+
            str(charge_current))
  if value != None:
      charge_current = float(value)
      scpi('DISP:DIALog:DATA "charge_current", FLOAT, AMPER,' +
        str(charge_current))

def input_reform_time():
  global reform_time
  value = scpi('DISP:INPUT? "Reform duration",NUMBER,SECOND,10.0,7200.0,'+
            str(reform_time))
  if value != None:
      reform_time = float(value)
      scpi('DISP:DIALog:DATA "reform_time", FLOAT, SECOND,' +
        str(reform_time))

def close_script():
  # Turn off channels, etc.
  scpi("OUTP 0")
  scpi("DISP:DIAL:CLOSE")
  scpi("*RCL 10")
  scpi("MEM:STATE:FREEZE OFF")

def start_reform():
  global max_cap_volt, charge_current, reform_time, have_data, channel
  
  try:
    scpi("SENS:CURR:RANG BEST")
    scpi("VOLT 0")
    scpi("CURR 0")
    scpi("CURR " + str(charge_current))
    
    # Define datalogging
    scpi("SENS:DLOG:CLE")
    scpi('SENS:DLOG:TRAC:REM " Icharge = ' + str(charge_current) + '"')
    scpi("SENS:DLOG:TRAC:X:UNIT SECOND")
    scpi("SENS:DLOG:TRAC:X:STEP " + str(TIME_STEP))
    scpi("SENS:DLOG:TRAC:X:RANG:MAX " + str(reform_time))
    scpi('SENS:DLOG:TRAC:X:LABel "Time"')
    scpi("SENS:DLOG:TRAC:Y1:UNIT VOLT")
    scpi("SENS:DLOG:TRAC:Y1:RANG:MAX " + str(cap_max_volt*1.1))
    scpi("SENS:DLOG:TRAC:Y1:RANG:MIN 0")
    scpi('SENS:DLOG:TRAC:Y1:LABel "Vmon"')
    scpi("SENS:DLOG:TRAC:Y2:UNIT AMPER")
    scpi("SENS:DLOG:TRAC:Y2:RANG:MAX " + str(charge_current*2))
    scpi("SENS:DLOG:TRAC:Y2:RANG:MIN 0")
    scpi('SENS:DLOG:TRAC:Y2:LABel "Imon"')
    scpi('INIT:DLOG:TRACE "/Recordings/ReformCap.dlog"')

    scpi("OUTP 1")
    have_data = True
      
    starttime = ticks_ms()
    t = starttime
    tdiff = 0.0  # Allocate outside loop for speed.
    action = ""
    
    # This is the measurement loop.
    ###############################
    while True:
      scpi('SYST:DIG:OUTP:DATA 4,1')  # For timing analysis
      # Time critical stuff first
      ###########################
      # Measure as soon and fast as possible to hit it as close as possible to the set time (after delay)
      uMon = getU(channel)
      iMon = getI(channel)
      
      # Remember timestamp of the beginning of this iteration
      nowtime = ticks_ms()
      
      # Set (new) value    
      uSet = STEP1 * cap_max_volt 
    
      if nowtime > (starttime + 0.6666 * reform_time * 1000):
        uSet = STEP3 * cap_max_volt
      elif nowtime > (starttime + 0.3333 * reform_time * 1000):
        uSet = STEP2 * cap_max_volt
      setU(channel,uSet)

      # Not time critical
      ###################
      dlogTraceData(uMon,iMon)
      
      # Done with loop?
      if nowtime > (starttime + reform_time * 1000):
        scpi('SYST:DIG:OUTP:DATA 4,0')
        break 

      # Finally do the slow GUI stuff.
      ################################
      # Process the action read during the delay
      if action == "view_dlog":
        scpi("DISP:WINDOW:DLOG")
      
      if action == "stop_reform":
        break

      # Update progress bar.
      scpi("DISP:DIAL:DATA \"reform_progress\", INT, " + str(int((nowtime-starttime)/(reform_time*10))))
      # Update V and I data
      scpi("DISP:DIAL:DATA \"Vmon\", FLOAT, VOLT, " + str(uMon))
      scpi("DISP:DIAL:DATA \"Imon\", FLOAT, AMPER, " + str(iMon))
      
      # Loop delay
      ############
      # Set the next time we want the loop to execute
      t = ticks_add(t, int(TIME_STEP * 1000))      
      scpi('SYST:DIG:OUTP:DATA 4,0')
      # Everything after this line is time sensitive, it is nog compensated in the timing calculation
      # It will cause a drift in sampling time.
      # Calculate how long until next interval, wait for this time in dialog wait.
      #sleep_ms(ticks_diff(t,ticks_ms()))
      action = scpi("DISP:DIALOG:ACTION? " + str(max(1,ticks_diff(t,ticks_ms()))) + 'ms')
      # We don't process the action here but in the next iteration to keep timing
      # of measurements as jitter free as possible
      
  
  finally:
    scpi("OUTP 0")
    # Stop recording.
    scpi('ABOR:DLOG')
    discharge_cap()
    

def discharge_cap():
  global channel
  setU(channel, 0.0)
  setI(channel, 0.1) # Must be >= 10mA otherwise Downprogrammer is off.
  ovp = scpi("SOUR1:VOLT:PROT:STAT?")
  if ovp:
    scpi("SOUR1:VOLT:PROT:STAT OFF")
  scpi("DISP:WINDOW:TEXT \"Discharging...\"")
  scpi("OUTP 1")
  sleep_ms(1000)
  while getU(channel) > 0.5:
    sleep_ms(100)
  scpi("DISP:WINDOW:TEXT:CLEAR")
  setI(channel, 0.1)
  scpi("OUTP 0")
  if ovp:
    scpi("SOUR1:VOLT:PROT:STAT ON")
  
def show_main_dialog():
  global have_data
  scpi("DISP:DIAL:OPEN \"Scripts/CapReform.res\"")
  scpi('DISP:DIALog:DATA "cap_max_volt", FLOAT, VOLT,' +
      str(cap_max_volt))
  scpi('DISP:DIALog:DATA "charge_current", FLOAT, AMPER,' +
      str(charge_current))
  scpi('DISP:DIALog:DATA "reform_time", FLOAT, SECOND,' +
    str(reform_time))
  scpi('DISP:DIALog:DATA "data_viewable", INT, ' + str(int(have_data)))  
  scpi('DISP:DIALog:DATA "run_state", INT, 0')
  
  
# Start of main (loop) script
#############################
def main():
  global module_max_volt
  # Save state  
  scpi("*SAV 10")
  scpi("MEM:STATE:FREEZE ON")

  # From now on we always restore the state in case of an error.  
  try:
    # Script requires firmware > 1.6, check for it.
    firmwareversion = scpi("SYSTem:CPU:FIRMware?")
    if float(firmwareversion) < 1.6:
      scpi('DISP:ERR "Script requires firmware >= 1.6"')
      return
  
    # Digital output for timing/jitter measurements
    scpi('SYSTEM:DIGITAL:PIN4:FUNCTION DOUTPUT')
    scpi('SYST:DIGITAL:PIN4:POLARITY POS')
    scpi('SYST:DIG:OUTP:DATA 4,0')
  
    if scpi("SYSTem:CHANnel:COUNt?") >= 2:
      ch1Model = scpi("SYSTem:CHANnel:MODel? ch1")
      ch2Model = scpi("SYSTem:CHANnel:MODel? ch2")
      if ch1Model.startswith("DCP405") and ch2Model.startswith("DCP405"):
        scpi("INST:COUP:TRAC SER")
      else:
        scpi("INST:COUP:TRAC NONE")
    else:
      scpi("INST:COUP:TRAC NONE")
      
    scpi("INST ch1")
    module_max_volt = float(scpi("VOLT? MAX"))
    scpi("OUTP 0")
    show_main_dialog()

    while True:
        action = scpi("DISP:DIALOG:ACTION?")
        if action == "input_cap_max_volt":
          input_cap_max_volt()
        elif action == "input_charge_current":
          input_charge_current()
        elif action == "input_reform_time":
          input_reform_time()
        elif action == "input_reform_time":
          input_reform_time()
        elif action == "view_dlog":
          scpi("DISP:WINDOW:DLOG \"/Recordings/ReformCap.dlog\"")
        elif action == "start_reform":
          scpi('OUTP:PROT:CLE CH1')
          scpi('OUTP:PROT:CLE CH2')
          scpi("INST CH1")
          scpi('OUTP:DPROG ON')
          discharge_cap()
          scpi('DISP:DIALog:DATA "run_state", INT, 1')
          scpi('DISP:DIALog:DATA "data_viewable", INT, 1')  
          start_reform()
          scpi("DISP:DIAL:DATA \"reform_progress\", INT, 0")
          scpi('DISP:DIALog:DATA "run_state", INT, 0')
        elif action == "close_script" or action == 0:
            break
  finally:
    close_script()


main()
    
         
          

