/*
* An example of how to initiate DLOG session and record voltage and current on CH1
* WARNING: CH1 output current will be set to 2 A, and voltage will vary between 5 and 10 V.
* Please use simple load such as power resistor for the test.
*
* Tested with Arduino Mega 2560 and Due. Use UART1: pin 18 -> BB3 DIN1, pin 19 -> BB3 DOUT1
*
*/

void setup() {
    // setup serial
    Serial1.begin(115200, SERIAL_8E2);
  
    // Intro message
    delay(1000);
    Serial1.println("DISP:TEXT 'Arduino here, you are under my control!'");
    delay(2000);
    Serial1.println("DISP:TEXT:CLE"); // clear message
    delay(1000);
    
    // init dlog
    Serial1.println("SENS:DLOG:PER 0.01");
    Serial1.println("SENS:DLOG:TIME 60");
    Serial1.println("SENS:DLOG:FUNC:VOLT ON, CH1");
    Serial1.println("SENS:DLOG:FUNC:CURR ON, CH1");
    Serial1.println("INIT:DLOG '/Recordings/arduino-dlog-bookmarks.dlog'");
    // set CH1 params
    Serial1.println("INST CH1");
    Serial1.println("CURR 2");
    Serial1.println("OUTP 1");

    // make sure all previous commands are executed
    Serial1.println("*OPC?");
    while (!Serial1.available());

    // wait 1sec
    delay (1000);

    // add "start" bookmark
    Serial1.println("SENS:DLOG:TRAC:BOOK 'Start'");

    // change output voltage 5 times on CH1
    for (int i = 0; i < 5; i++) {
        Serial1.println("VOLT 10");
        Serial1.println("SYST:DEL 500");
        Serial1.println("VOLT 5");
        Serial1.println("SYST:DEL 1000");
    }

    // add "pause" bookmark
    Serial1.println("SENS:DLOG:TRAC:BOOK 'Pause'");
    delay (3000);

    // add "stop" bookmark
    Serial1.println("SENS:DLOG:TRAC:BOOK 'Stop'");
    
    // wait 1sec
    delay(1000);
    
    // abort dlog
    Serial1.println("ABOR:DLOG");
    
    // Goodbye message
    delay(1000);
    Serial1.println("DISP:TEXT 'I am checking out, you are free again!'");
    delay(5000);
    Serial1.println("DISP:TEXT:CLE");  // clear message
}

void loop() {
}
