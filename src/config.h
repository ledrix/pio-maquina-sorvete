#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// #define RELEASE
#define DEBUG

// #define KUKA_ROBOT
#define NEUROMEKA_ROBOT

const char* name    = "ElectroFreeze";

IPAddress ETH_IP(192, 168, 15, 49);
byte ETH_MAC[]          = {0xE0, 0xE0, 0xE0, 0xE0, 0xE0, 0xE9};

const char* MQTT_IP     = "192.168.0.101";
const int   mqtt_rate   = 100; //Hz
const char* scale_topic = "Gripper/reading";

#ifdef KUKA_ROBOT
const char* kuka_IP       = "192.168.15.147";
const int   kuka_refresh  = 10; //Hz
#endif //KUKA

#ifdef NEUROMEKA_ROBOT
const char* neuromeka_IP      = "192.168.0.101";
const int   neuromeka_address = 950;
const int   neuromeka_refresh =  10; //Hz
#endif //NEUROMEKA

#endif //!CONFIG_H