from eez import scpi

VOLTS = [1.0, 2.0, 3.0, 4.0, 5.0]

old_pin_states = [0, 0]

def button_pressed(pin):
	old_pin_state = old_pin_states[pin]
	new_pin_state = scpi("SYSTem:DIGital:INPut:DATA? " + str(pin + 1))
	old_pin_states[pin] = new_pin_state
	return new_pin_state == 1 and old_pin_state == 0

scpi("DISPlay:HOMe")

pos = 0
while True:
	if button_pressed(0): # check if pin 1 is pressed
		# go forward
		if pos == len(VOLTS) - 1:
			# roll round to first value
			pos = 0
		else:
			pos = pos + 1
	elif button_pressed(1): # check if pin 2 is pressed
		# go backward
		if pos == 0:
			# roll round to last value
			pos = len(VOLTS) - 1
		else:
			pos = pos - 1
	else:
		continue # no pin pressed

	scpi("VOLT " + str(VOLTS[pos]))
