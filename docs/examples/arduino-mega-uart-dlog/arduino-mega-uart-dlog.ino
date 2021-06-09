void setup() {
    // setup serial
    Serial1.begin(115200, SERIAL_8E2);
    
    // setup LED
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

    // wait 1sec
    delay(1000);

    // init dlog
    Serial1.print("SENSe:DLOG:PERiod 0.001\n");
    Serial1.print("SENSe:DLOG:TIME 60\n");
    Serial1.print("SENS:DLOG:FUNC:VOLT ON, CH1\n");
    Serial1.print("SENS:DLOG:FUNC:CURR ON, CH1\n");
    Serial1.print("INITiate:DLOG '/Recordings/arduino-mega-uart-dlog.dlog'\n");

    // wait 1 sec
    delay(1000);

    // add "start blink" bookmark
    Serial1.print("SENS:DLOG:TRAC:BOOK 'start blink'\n");

    // blink 5 times
    for (int i = 0; i < 5; i++) {
        digitalWrite(LED_BUILTIN, HIGH);
        delay(500);
        digitalWrite(LED_BUILTIN, LOW);
        delay(500);
    }

    // add "stop blink" bookmark
    Serial1.print("SENS:DLOG:TRAC:BOOK 'stop blink'\n");
    
    // wait 1 sec
    delay(1000);
    
    // abort dlog
    Serial1.print("ABORT:DLOG\n");
}

void loop() {
}