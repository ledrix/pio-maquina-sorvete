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
TaskHandle_t kuka_taskhandle = nullptr;
TaskHandle_t lights_taskhandle = nullptr;
TaskHandle_t machine_taskhandle = nullptr;
TaskHandle_t neuromeka_taskhandle = nullptr;
TaskHandle_t statusLED_taskhandle = nullptr;
//------------------------------------------------


//Function Prototypes ----------------------------
void mqtt_task(void *parameters);
void kuka_task(void *parameters);
void lights_task(void *parameters);
void machine_task(void *parameters);
void neuromeka_task(void *parameters);
void statusLED_task(void *parameters);
//------------------------------------------------


//Interrupt Handlers -----------------------------
void IRAM_ATTR level_ISR()
{
  static TickType_t then = 0;
  TickType_t now = xTaskGetTickCountFromISR();

  if(!digitalRead(LEVEL))
    then = now;

  level = (now - then <= pdMS_TO_TICKS(100));
}
void IRAM_ATTR running_ISR()
{
  static TickType_t then = 0;
  TickType_t now = xTaskGetTickCountFromISR();

  if(!digitalRead(RUNNING))
    then = now;

  running = (now - then <= pdMS_TO_TICKS(100));
}
void IRAM_ATTR pressure_ISR()
{
  static TickType_t then = 0;
  TickType_t now = xTaskGetTickCountFromISR();

  if(!digitalRead(PRESSURE))
    then = now;

  pressure = (now - then <= pdMS_TO_TICKS(100));
}
void IRAM_ATTR temperature_ISR()
{
  static TickType_t then = 0;
  TickType_t now = xTaskGetTickCountFromISR();

  if(!digitalRead(TEMPERATURE))
    then = now;

  temperature = (now - then <= pdMS_TO_TICKS(100));
}
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

  pinMode(LEVEL, INPUT_PULLUP);
  pinMode(RUNNING, INPUT_PULLUP);
  pinMode(PRESSURE, INPUT_PULLUP);
  pinMode(TEMPERATURE, INPUT_PULLUP);

  attachInterrupt(LEVEL, level_ISR, CHANGE);
  attachInterrupt(RUNNING, running_ISR, CHANGE);
  attachInterrupt(PRESSURE, pressure_ISR, CHANGE);
  attachInterrupt(TEMPERATURE, temperature_ISR, CHANGE);

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

#ifdef KUKA_ROBOT
  xTaskCreatePinnedToCore(  kuka_task,
                            "KUKA Robot",
                            8192,
                            nullptr,
                            1,
                            &kuka_taskhandle,
                            ARDUINO_RUNNING_CORE );
#endif //KUKA_ROBOT

#ifdef NEUROMEKA_ROBOT
  xTaskCreatePinnedToCore(  neuromeka_task,
                            "Neuromeka Robot",
                            8192,
                            nullptr,
                            1,
                            &neuromeka_taskhandle,
                            ARDUINO_RUNNING_CORE );
#endif //NEUROMEKA_ROBOT

  xTaskCreatePinnedToCore(  machine_task,
                            "Machine Control",
                            1024,
                            nullptr,
                            1,
                            &machine_taskhandle,
                            ARDUINO_RUNNING_CORE );

  xTaskCreatePinnedToCore(  lights_task,
                            "LEDs & Buzzer",
                            1024,
                            nullptr,
                            1,
                            &lights_taskhandle,
                            ARDUINO_RUNNING_CORE );

  xTaskCreatePinnedToCore(  statusLED_task,
                            "Status LED",
                            1024,
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

#ifdef KUKA_ROBOT
void kuka_task(void *parameters)
{
  uint8_t i = 0;
  String response;
  uint16_t robot[] = {0, 0, 0, 0};
  uint16_t pRobot[] = {0, 0, 0, 0};

  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(1000/kuka_refresh));
    
    robot_connected = kuka.connected();
    if(robot_connected)
    {
      response = kuka.read(String(kuka_var) + "[" + i + "]");
      Serial.printf("%s[%d] = %s", kuka_var, i, response);

      if(response.indexOf("ERROR") != -1)
        continue;
      
      robot[i] = response.toInt();

      if(robot[i] > 0)
      {
        if(robot[i] == 1)
        {
          if(pRobot[i] == 0)
            motor[i] = 1;
          else if(motor[i] == 2)
          {
            if(!kuka.write(String(kuka_var) + "[" + i + "]", String(motor[i])))
              continue;
          }
        }
        else if(robot[i] == 2)
        {
          if(motor[i] == 3)
          {
            if(!kuka.write(String(kuka_var) + "[" + i + "]", String(motor[i])))
              continue;
          }
        }
        else if(robot[i] == 3)
        {
          if(motor[i] == 0)
          {
            if(!kuka.write(String(kuka_var) + "[" + i + "]", String(motor[i])))
              continue;
          }
          else if(motor[i] == 2)
            motor[i] = 3;
        }
      }
      else
        i = (i + 1 >= 4) ? 0 : i + 1;

      pRobot[i] = robot[i];
    }
    else
    {
      Serial.println("[KUKA]: Connecting...");
      if(kuka.connect())
        Serial.println("[KUKA]: Connecting...OK");
      else
        Serial.println("[KUKA]: Connecting...FAIL");
    }
  }
}
#endif //KUKA_ROBOT


#ifdef NEUROMEKA_ROBOT
void neuromeka_task(void *parameters)
{
  IPAddress IP;
  IP.fromString(neuromeka_IP);

  uint16_t robIn[] = {0, 0, 0, 0, 0};
  uint16_t robOut[] = {0, 0, 0, 0, 0};

  modbus.client();
  for(;;)
  {
    robot_connected = modbus.isConnected(IP);
    if(!robot_connected)
    {
      Serial.println("[Neuromeka]: Connecting...");
      if(!modbus.connect(IP, 502))
      {
        Serial.println("[Neuromeka]: Connecting...FAIL");
        vTaskDelay(pdMS_TO_TICKS(100));
        continue;
      }
      Serial.println("[Neuromeka]: Connecting...OK");
    }
    else
    {
      modbus.task();
      
      modbus.readHreg(IP, neuromeka_address, robIn, 5);
      spinner     = robIn[0];
      mixer       = robIn[1];
      shake_ready = robIn[2];

      robOut[0] = level;
      robOut[1] = pressure;
      robOut[2] = temperature;
      robOut[3] = running;
      robOut[4] = scale_connected;
      modbus.writeHreg(IP, neuromeka_address, robOut, 5);
    }
    vTaskDelay(pdMS_TO_TICKS(1000/neuromeka_refresh));
  }
}
#endif //NEUROMEKA_ROBOT


void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.printf("[MQTT]: Message arrived @ %s --> ", topic);

  String message = String((char*)payload).substring(0, length);

  Serial.println(message);

  if(message.indexOf("ping") != -1)
    mqtt.publish("dev/monitor",("[" + String(name) + "]: Pong!").c_str());

  if(String(topic).indexOf(scale_topic) != -1)
    scale_lastComm = xTaskGetTickCount();
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


void lights_task(void *parameters)
{
  TickType_t now = 0;
  TickType_t then_scale = 0;
  TickType_t then_ready = 0;

  bool pShakeReady = false;

  for(;;)
  {
    now = xTaskGetTickCount();

    digitalWrite(SIGN_YLW, level);

    if(now - then_scale >= pdMS_TO_TICKS(500))
    {
      digitalWrite(SIGN_BLE, scale_connected && !digitalRead(SIGN_BLE));
      then_scale = now;
    }

    if(shake_ready && !pShakeReady)
    {
      digitalWrite(SIGN_GRN, HIGH);
      then_ready = now;
    }
    pShakeReady = shake_ready;
    if(now - then_ready >= pdMS_TO_TICKS(5000))
      digitalWrite(SIGN_GRN, LOW);

    vTaskDelay(pdMS_TO_TICKS(10));
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