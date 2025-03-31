#ifndef PCB_H
#define PCB_H

#include <Arduino.h>

//Status Sign
const int SIGN_RED = 32;
const int SIGN_GRN = 33;
const int SIGN_BLE = 25;
const int SIGN_YLW = 14;
const int SIGN_BZR = 13;

//Outputs
const int MIXER = 27;
const int SPINNER = 26;

//Inputs
const int LEVEL = 35;
const int RUNNING = 34;
const int PRESSURE = 36;
const int TEMPERATURE = 39;

const int LED = 4;

const int ETH_CS = 5;

#ifndef LED_BUILTIN
    #define LED_BUILTIN 2   //ESP32 DevKit 1
#endif

#endif //!PCB_H