#include <IRLibSendBase.h>  // We need the base code
#include <IRLib_HashRaw.h>  // Only use raw sender

IRsendRaw mySender;         // Initialize IR emitter

const uint16_t RAW_DATA_LEN = 68;

// Power Samsung TV
uint16_t rawData[RAW_DATA_LEN] = {4600,4350,700,1550,650,1550,650,1600,650,450,650,450,650,450,650,450,700,400,700,1550,650,1550,650,1600,650,450,650,450,650,450,700,450,650,450,650,450,650,1550,700,450,650,450,650,450,650,450,650,450,700,400,650,1600,650,450,650,1550,650,1600,650,1550,650,1550,700,1550,650,1550,650};


void setup()
{
    Serial.begin(9600);
    delay(2000);
    while (!Serial); // Delay for Leonardo.
    Serial.println(F("Every time you press a key is a serial monitor we will send."));
}


void loop()
{
    if (Serial.read() != -1)
    {
        /* 
        Send a code every time a character is received from the
        Serial port. You could modify this sketch to send when you
        Push a button connected to an digital input pin.
        */
        mySender.send(rawData, RAW_DATA_LEN, 38); // Pass the buffer, length, optionally frequency
        Serial.println(F("Sent signal."));
    }
}
