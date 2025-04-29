#include "pcb.h"
#include "config.h"
#include "version.h"

//Arduino -------------------

//WIFI ----------------------
#include <ArduinoOTA.h>
#include <WiFiManager.h>
//Ethernet ------------------
#include <EthernetESP32.h>
//Network -------------------
#ifdef KUKA_ROBOT
#include <kuka.h>
#endif //KUKA_ROBOT
#ifdef NEUROMEKA_ROBOT
#include <ModbusEthernet.h>
#endif //NEUROMEKA_ROBOT
#include <ESPmDNS.h>
#include <WebServer.h>
#include <PubSubClient.h>


//Objects ----------------------------------------
WiFiManager wifi;
W5500Driver w5500(ETH_CS);
NetworkClient mqtt_client;
PubSubClient mqtt(mqtt_client);
#ifdef KUKA_ROBOT
NetworkClient kuka_client;
KUKA kuka(kuka_IP, 7000, kuka_client);
#endif //KUKA_ROBOT
#ifdef NEUROMEKA_ROBOT
ModbusEthernet modbus;
#endif //NEUROMEKA_ROBOT
//------------------------------------------------


//Global Variables -------------------------------
volatile bool level = false;
volatile bool running = false;
volatile bool pressure = false;
volatile bool temperature = false;

volatile bool spinner = false;
volatile bool mixer = false;

bool mqtt_connected = false;
bool robot_connected = false;
bool ethernet_connected = false;

bool shake_ready = false;

bool scale_connected = false;
TickType_t scale_lastComm = 0;

TaskHandle_t mqtt_taskhandle = nullptr;
//TaskHandle_t kuka_taskhandle = nullptr;
//TaskHandle_t lights_taskhandle = nullptr;
TaskHandle_t machine_taskhandle = nullptr;
//TaskHandle_t neuromeka_taskhandle = nullptr;
TaskHandle_t statusLED_taskhandle = nullptr;
//------------------------------------------------


//Function Prototypes ----------------------------
void mqtt_task(void *parameters);
//void kuka_task(void *parameters);
//void lights_task(void *parameters);
void machine_task(void *parameters);
//void neuromeka_task(void *parameters);
void statusLED_task(void *parameters);
//------------------------------------------------


void setup() 
{
  pinMode(SIGN_RED, OUTPUT);
  pinMode(SIGN_GRN, OUTPUT);
  pinMode(SIGN_BLE, OUTPUT);
  pinMode(SIGN_YLW, OUTPUT);
  pinMode(SIGN_BZR, OUTPUT);

  pinMode(LED, OUTPUT);
  pinMode(MIXER, OUTPUT);
  pinMode(SPINNER, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  pinMode(LEVEL, INPUT_PULLUP);
  pinMode(RUNNING, INPUT_PULLUP);
  pinMode(PRESSURE, INPUT_PULLUP);
  pinMode(TEMPERATURE, INPUT_PULLUP);

  attachInterrupt(LEVEL,       []() IRAM_ATTR { level       = !digitalRead(LEVEL);       }, CHANGE);
  attachInterrupt(RUNNING,     []() IRAM_ATTR { running     = !digitalRead(RUNNING);     }, CHANGE);
  attachInterrupt(PRESSURE,    []() IRAM_ATTR { pressure    = !digitalRead(PRESSURE);    }, CHANGE);
  attachInterrupt(TEMPERATURE, []() IRAM_ATTR { temperature = !digitalRead(TEMPERATURE); }, CHANGE);

  Serial.begin(115200);
  Serial.printf("\nHi there! My name is %s.\n", name);
  Serial.printf("Firmware @ %s\n", VERSION_STR);

  // Initialize Ethernet
  Ethernet.setHostname(name);
  Ethernet.init(w5500);
  Ethernet.begin();
  if(Ethernet.hardwareStatus() == EthernetNoHardware)
    Serial.println("[Ethernet]: Hardware not found!");
  else if(Ethernet.linkStatus() == LinkOFF)
    Serial.println("[Ethernet]: Cable not connected!");
  else
    Serial.printf("[Ethernet]: IP = %s\n", Ethernet.localIP().toString());

  // FreeRTOS
  xTaskCreatePinnedToCore(  mqtt_task,
                            "MQTT Communication",
                            8192,
                            nullptr,
                            1,
                            &mqtt_taskhandle,
                            ARDUINO_RUNNING_CORE );

  xTaskCreatePinnedToCore(  machine_task,
                            "Machine Control",
                            8192,
                            nullptr,
                            1,
                            &machine_taskhandle,
                            ARDUINO_RUNNING_CORE );
  
  xTaskCreatePinnedToCore(  statusLED_task,
                            "Status LED",
                            8192,
                            nullptr,
                            1,
                            &statusLED_taskhandle,
                            ARDUINO_RUNNING_CORE );
  
  // WiFi
  wifi.setHostname(name);
  wifi.setDarkMode(true);
  wifi.setTitle("Robotine");
  #ifdef RELEASE
  wifi.setConnectTimeout(5 * 60);
  wifi.setConfigPortalTimeout(5 * 60);
  #endif //RELEASE
  wifi.setConfigPortalBlocking(false);
  wifi.autoConnect(name);  

  // mDNS Service
  MDNS.begin(name);
  MDNS.addService("http", "tcp", 80);

  // OTA Updates
  ArduinoOTA.setHostname(name);
  ArduinoOTA.begin();
}

void loop()
{
  ArduinoOTA.handle();

  if(!WiFi.isConnected())
  {
    wifi.process();

    // static uint32_t then = millis();
    // if(millis() - then >= 1000)
    // {
    //   WiFi.reconnect();
    //   then = millis();
    // }
  }
}

void callback(char* topic_array, byte* payload, unsigned int length) 
{
  String topic = topic_array;
  String message = String((char*)payload).substring(0, length);
  Serial.printf("[MQTT]: Message arrived @ %s --> %s\n", topic, message);

  if(message.indexOf("ping") != -1)
    mqtt.publish("dev/monitor",("[" + String(name) + "]: Pong!").c_str());

  if(topic == scale_topic)
    scale_lastComm = xTaskGetTickCount();

  if(topic.endsWith("mixer"))
    mixer = message == "ON";

  if(topic.endsWith("spinner"))
    spinner = message == "ON";
    
  if(topic.endsWith("finished"))
    shake_ready = message == "ON";
}

void mqtt_task(void *parameters)
{
  mqtt.setServer(MQTT_IP, 1883);
  mqtt.setCallback(callback);

  for(;;)
  {
    mqtt_connected = mqtt.connected();
    if(!mqtt_connected)
    {
      Serial.println("[MQTT]: Connecting...");
      if(mqtt.connect(name))
      {
        Serial.println("[MQTT]: Connecting...OK");

        mqtt.subscribe((String(name) + "/mixer").c_str(), 0);
        mqtt.subscribe((String(name) + "/spinner").c_str(), 0);
        mqtt.subscribe((String(name) + "/finished").c_str(), 0);

        mqtt.subscribe(scale_topic, 0);
        
        mqtt.subscribe("dev/data", 0);

        if(ethernet_connected)
          mqtt.publish("dev/monitor", ("[" + String(name) + "]: Ethernet").c_str());
        else
          mqtt.publish("dev/monitor", ("[" + String(name) + "]: WiFi").c_str());
      }
      else
      {
        Serial.printf("[MQTT]: Connecting...FAIL (%d)\n", mqtt.state());
        delay(1000);
      }
    }
    else
    {
      mqtt.loop();
      
      mqtt.publish((String(name) + "/level").c_str()      ,       level ? "TRUE" : "FALSE");
      mqtt.publish((String(name) + "/running").c_str()    ,     running ? "TRUE" : "FALSE");
      mqtt.publish((String(name) + "/pressure").c_str()   ,    pressure ? "TRUE" : "FALSE");
      mqtt.publish((String(name) + "/temperature").c_str(), temperature ? "TRUE" : "FALSE");

      scale_connected = (xTaskGetTickCount() - scale_lastComm < pdMS_TO_TICKS(1000));

      vTaskDelay(pdMS_TO_TICKS(1000/mqtt_rate));
    }
  }
}

void machine_task(void *parameters)
{
  for(;;)
  {
    digitalWrite(SPINNER, spinner);

    //Probably needs control logic to only be on for a certain time
    digitalWrite(MIXER, mixer);

    vTaskDelay(10);
  }
}

void statusLED_task(void *parameters)
{
  byte blynks;

  for(;;)
  {
    ethernet_connected = Ethernet.connected();

    blynks = 1 
           + 1 * mqtt_connected 
           + 2 * robot_connected;
    for(int i = 0; i < blynks; i++)
    {
      digitalWrite(LED_BUILTIN, HIGH);
      vTaskDelay(pdMS_TO_TICKS(100));
      digitalWrite(LED_BUILTIN, LOW);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}