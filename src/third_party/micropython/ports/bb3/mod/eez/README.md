# `eez` Module

This module contains following functions:

---
`eez.scpi(commandOrQuery)`

Execute any SCPI command or query. If command is executed then None is returned. If query is executed then it returns query result as integer or string.

---
`eez.getU(channelIndex)`

Returns measured voltage as float for the given channel index.

This is same as `MEASure[:SCALar]:VOLTage[:DC]?` SCPI query. Use this function instead of `scpi` function when performance requirement is critical.

---
`eez.setU(channelIndex, currentLevel)`

This function sets the immediate voltage level for the given channel index.

This is same as `[SOURce[<n>]]:VOLTage[:LEVel][:IMMediate][:AMPLitude]` SCPI command. Use this function instead of `scpi` function when performance requirement is critical.

---
`eez.getI(channelIndex)`

Returns measured current as float for the given channel index. 

This is same as `MEASure[:SCALar]:CURRent[:DC]?` SCPI query. Use this function instead of `scpi` function when performance requirement is critical.

---
`eez.setI(channelIndex, currentLevel)`

This function sets the immediate current level for the given channel index.

This is same as `[SOURce[<n>]]:CURRent[:LEVel][:IMMediate][:AMPLitude]` SCPI command. Use this function instead of `scpi` function when performance requirement is critical.

---
`eez.getOutputMode(channelIndex)`

For the given channel index, returns:

- `"CV"` when channel is in Constant Voltage mode
- `"CC"` when channel is in Constant Current mode
- `"UR"` when channel is neither in Constant Voltage or Constant Current mode

This is same as `OUTPut:MODE?` SCPI query. Use this function instead of `scpi` function when performance requirement is critical.

---
`eez.dlogTraceData(value, ...)`

For current DLOG trace file, this function adds one point in time for each defined Y-axis. It expects one or more value arguments depending of how much Y-axis values are defined for currently started DLOG trace.

This is same as `SENSe:DLOG:TRACe[:DATA]` SCPI command. Use this function instead of `scpi` function when performance requirement is critical.
