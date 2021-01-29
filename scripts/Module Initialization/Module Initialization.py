# Module initialization utility

from utime import ticks_ms, ticks_add, ticks_diff, sleep_ms
from eez import scpi, setU, getOutputMode, getI

MODEL_NONE = 0
MODEL_DCP405  = 1
MODEL_DCM220  = 2
MODEL_DCM224  = 3

MODELS = [
    { "name": "None",    "id": 0,   "r": 0, "b": 0  },
    { "name": "DCP405",  "id": 405, "r": 2, "b": 11 },
    { "name": "DCM220",  "id": 220, "r": 2, "b": 8  },
    { "name": "DCM224",  "id": 224, "r": 1, "b": 1  },
    { "name": "MIO168",  "id": 168, "r": 1, "b": 2  },
    { "name": "PREL6",   "id": 6,   "r": 1, "b": 2  },
    { "name": "SMX46",   "id": 46,  "r": 1, "b": 2  },
]

slots = [
    { "model": MODEL_NONE, "version_r": 0, "version_b": 0 },
    { "model": MODEL_NONE, "version_r": 0, "version_b": 0 },
    { "model": MODEL_NONE, "version_r": 0, "version_b": 0 },
]

slots_orig = []

can_restart = 0

def error(message):
    scpi('DISP:ERROR "' + message + '"')

def update_model_and_version(slot_index):
    scpi("DISP:DIALog:DATA \"slot" + str(slot_index + 1) + "_model\",STR,\"" + MODELS[slots[slot_index]["model"]]["name"] + "\"")
    scpi("DISP:DIALog:DATA \"slot" + str(slot_index + 1) + "_version_r\",INT," + str(slots[slot_index]["version_r"]))
    scpi("DISP:DIALog:DATA \"slot" + str(slot_index + 1) + "_version_b\",INT," + str(slots[slot_index]["version_b"]))

def update_can_init(slot_index):
    if slots[slot_index]["model"] != slots_orig[slot_index]["model"] or slots[slot_index]["version_r"] != slots_orig[slot_index]["version_r"] or slots[slot_index]["version_b"] != slots_orig[slot_index]["version_b"]:
        scpi("DISP:DIALog:DATA \"slot" + str(slot_index + 1) + "_can_init\",INT,1")
    else:
        scpi("DISP:DIALog:DATA \"slot" + str(slot_index + 1) + "_can_init\",INT,0")
    
def select_model(slot_index):
    model_names = ",".join(map(lambda x: '"' + x["name"] + '"', MODELS))
    value = scpi('DISP:SELECT? ' + str(slots[slot_index]["model"] + 1) + ',' + model_names)
    if value != None and value != 0:
        value = value - 1
        slots[slot_index]["model"] = value
        slots[slot_index]["version_r"] = MODELS[value]["r"]
        slots[slot_index]["version_b"] = MODELS[value]["b"]
        update_model_and_version(slot_index)
        update_can_init(slot_index)

def select_version_r(slot_index):
    value = scpi('DISP:INPUT? "",INT,0,255,' + str(slots[slot_index]["version_r"]))
    if value != None:
        slots[slot_index]["version_r"] = value
        update_model_and_version(slot_index)
        update_can_init(slot_index)

def select_version_b(slot_index):
    value = scpi('DISP:INPUT? "",INT,0,255,' + str(slots[slot_index]["version_b"]))
    if value != None:
        slots[slot_index]["version_b"] = value
        update_model_and_version(slot_index)
        update_can_init(slot_index)

def to_hex_str(num):
    hex_str = hex(num)[2:]
    return "0" + hex_str if len(hex_str) == 1 else hex_str

def slot_init(slot_index):
    try:
        scpi("INST:MEMO " + str(slot_index + 1) + ",0,2," + str(MODELS[slots[slot_index]["model"]]["id"]))
        scpi("INST:MEMO " + str(slot_index + 1) + ",2,2,#H" + to_hex_str(slots[slot_index]["version_r"]) + to_hex_str(slots[slot_index]["version_b"]))
        scpi("INST:MEMO " + str(slot_index + 1) + ",4,2,#HFFFF") # firmware is not installed
    except:
        error("Failed to write to module EEPROM.")
        return
    
    slots_orig[slot_index]["model"] = slots[slot_index]["model"]
    slots_orig[slot_index]["version_r"] = slots[slot_index]["version_r"]
    slots_orig[slot_index]["version_b"] = slots[slot_index]["version_b"]
    
    update_can_init(slot_index)

    global can_restart
    if not can_restart:
        can_restart = 1
        scpi("DISP:DIALog:DATA \"can_restart\",INT,1")

def get_slots_info():
    for slot_index in range(3):
        model = scpi("SYST:SLOT:MOD? " + str(slot_index + 1))
        for i in range(len(MODELS)):
            if MODELS[i]["name"] == model:
                slots[slot_index]["model"] = i

                version = scpi("SYST:SLOT:VERS? " + str(slot_index + 1))
                i = version.index('R')
                j = version.index('B')
                slots[slot_index]["version_r"] = int(version[i+1:j])
                slots[slot_index]["version_b"] = int(version[j+1:])
                
                break
        else:
            slots[slot_index]["model"] = MODEL_NONE
            slots[slot_index]["version_r"] = 0
            slots[slot_index]["version_b"] = 0

    global slots_orig
    slots_orig = [
        { "model": slots[0]["model"], "version_r": slots[0]["version_r"], "version_b": slots[0]["version_b"] },
        { "model": slots[1]["model"], "version_r": slots[1]["version_r"], "version_b": slots[1]["version_b"] },
        { "model": slots[2]["model"], "version_r": slots[2]["version_r"], "version_b": slots[2]["version_b"] },
    ]

get_slots_info()

scpi("DISP:DIALog:OPEN \"/Scripts/Module Initialization.res\"")

try:
    update_model_and_version(0)
    update_model_and_version(1)
    update_model_and_version(2)

    while True:
        action = scpi("DISP:DIALog:ACTIon?")
        if action == "slot1_select_model":
            select_model(0)
        elif action == "slot2_select_model":
            select_model(1)
        elif action == "slot3_select_model":
            select_model(2)
        elif action == "slot1_input_version_r":
            select_version_r(0)
        elif action == "slot1_input_version_b":
            select_version_b(0)
        elif action == "slot2_input_version_r":
            select_version_r(1)
        elif action == "slot2_input_version_b":
            select_version_b(1)
        elif action == "slot3_input_version_r":
            select_version_r(2)
        elif action == "slot3_input_version_b":
            select_version_b(2)
        elif action == "slot1_init":
            slot_init(0)
        elif action == "slot2_init":
            slot_init(1)
        elif action == "slot3_init":
            slot_init(2)
        elif action == "restart":
            scpi("SYST:RES")
        elif action == "close" or action == 0:
            break
finally:
    scpi("DISP:DIALog:CLOSe")