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
WebServer server(80);
#endif //NEUROMEKA_ROBOT
//------------------------------------------------


//Global Variables -------------------------------
volatile bool bloqueia_opcoes = false;
volatile bool libera_xarope = false;

char selecao = '0';
char selecionado = '0';

bool mqtt_connected = false;
bool robot_connected = false;
bool ethernet_connected = false;

bool shake_ready = false;

bool scale_connected = false;
TickType_t scale_lastComm = 0;

TaskHandle_t mqtt_taskhandle = nullptr;
TaskHandle_t statusLED_taskhandle = nullptr;
TaskHandle_t machine_taskhandle = nullptr;
//------------------------------------------------


//Function Prototypes ----------------------------
void mqtt_task(void *parameters);
void statusLED_task(void *parameters);
void machine_task(void *parameters);
//------------------------------------------------

IPAddress ip(192, 168, 15, 10);
IPAddress gateway(192, 168, 15, 1);
IPAddress subnet(255, 255, 255, 0);

void setup() 
{
  pinMode(BOMBA_XAROPE_3, OUTPUT);
  pinMode(SIGN_GRN, OUTPUT);
  pinMode(SIGN_BLE, OUTPUT);
  pinMode(SIGN_YLW, OUTPUT);
  pinMode(SIGN_BZR, OUTPUT);

  pinMode(LED, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BOMBA_XAROPE_2, OUTPUT);
  pinMode(BOMBA_XAROPE_1, OUTPUT);  
  
  pinMode(SINAL_BLOQUEIO_ROBO, INPUT_PULLUP);
  pinMode(SINAL_LIBERA_XAROPE, INPUT_PULLUP);
  
  attachInterrupt(SINAL_BLOQUEIO_ROBO,    []() IRAM_ATTR { bloqueia_opcoes    = !digitalRead(SINAL_BLOQUEIO_ROBO);    }, CHANGE);
  attachInterrupt(SINAL_LIBERA_XAROPE, []() IRAM_ATTR { libera_xarope = !digitalRead(SINAL_LIBERA_XAROPE); }, CHANGE);

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
  xTaskCreatePinnedToCore(  machine_task,
                            "Machine Control",
                            8192,
                            nullptr,
                            1,
                            &machine_taskhandle,
                            ARDUINO_RUNNING_CORE );

  xTaskCreatePinnedToCore(  mqtt_task,
                            "MQTT Communication",
                            8192,
                            nullptr,
                            1,
                            &mqtt_taskhandle,
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
  wifi.setSTAStaticIPConfig(ip, gateway, subnet);
  wifi.setConfigPortalBlocking(false);
  wifi.autoConnect(name);  

  // mDNS Service
  MDNS.begin(name);
  MDNS.addService("http", "tcp", 80);
  
  server.on("/1", HTTP_GET, []() {
    selecao = '1';      
    server.send(200, "text/plain", "\n--> Selecionado: Limão Siciliano \n\n");   
  });

  server.on("/2", HTTP_GET, []() {
    selecao = '2';      
    server.send(200, "text/plain", "\n--> Selecionado: Frutas Vermelhas \n\n");   
  });

  server.on("/3", HTTP_GET, []() {
    selecao = '3';      
    server.send(200, "text/plain", "\n--> Selecionado: Maça Verde \n\n");   
  });

  server.on("/0", HTTP_GET, []() {
    digitalWrite(BOMBA_XAROPE_1, LOW);
    digitalWrite(BOMBA_XAROPE_2, LOW);
    digitalWrite(BOMBA_XAROPE_3, LOW);    
    server.send(200, "text/plain", "\n--> Saidas 1, 2 e 3 Desativadas!\n\n");
  });
  
  server.on("/wifireset", HTTP_GET, []() {
    server.send(200, "text/plain", "\n--> Wifi Resetado e Saidas Desativadas!\n\n");
    digitalWrite(BOMBA_XAROPE_1, LOW);
    digitalWrite(BOMBA_XAROPE_2, LOW);
    digitalWrite(BOMBA_XAROPE_3, LOW);
    delay(500);
    wifi.resetSettings();   
  });

  server.on("/ajuda", HTTP_GET, []() {
    server.send(200, "text/plain", String("192.168.15.10/1 = SABOR 1\n") +
                                   String("192.168.15.10/2 = SABOR 2\n") +
                                   String("192.168.15.10/3 = SABOR 1\n") +
                                   String("192.168.15.10/wifireset = RESET WIFI\n\n"));       
  });

  // OTA Updates
  ArduinoOTA.setHostname(name);
  ArduinoOTA.begin();
  server.begin();
}

void loop()
{
  ArduinoOTA.handle();
  server.handleClient();

  if(!WiFi.isConnected())
  {
    wifi.process();    
  }
}

void machine_task(void *parameters)
{  

  for(;;)
  { 
    if (!bloqueia_opcoes) {
      selecionado = selecao;
    }

    if (libera_xarope) {
      while(libera_xarope) {
        switch (selecionado)
        {
        case '1':
          digitalWrite(BOMBA_XAROPE_1, HIGH);
          break;
        case '2':
          digitalWrite(BOMBA_XAROPE_2, HIGH);
          break;
        case '3':
          digitalWrite(BOMBA_XAROPE_3, HIGH);
          break;
        
        default:
          break;
        }        
        delay(100);
      }                  
    } else { 
      digitalWrite(BOMBA_XAROPE_1, LOW);
      digitalWrite(BOMBA_XAROPE_2, LOW);
      digitalWrite(BOMBA_XAROPE_3, LOW);            
    }
    vTaskDelay(10);
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
  /*
  if(topic.endsWith("spinner"))
    sabor_1 = message == "ON";

  if(topic.endsWith("mixer"))
    sabor_2 = message == "ON";
    
  if(topic.endsWith("finished"))
    shake_ready = message == "ON";
  */
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

        mqtt.subscribe((String(name) + "/sabor_1").c_str(), 0);        

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
      String selected = String(selecionado);
      String selection = String(selecao);
      mqtt.publish((String(name) + "/bloqueia_opcoes").c_str(), bloqueia_opcoes ? "TRUE" : "FALSE");
      mqtt.publish((String(name) + "/libera_xarope").c_str(), libera_xarope ? "TRUE" : "FALSE");
      mqtt.publish((String(name) + "/Selecionado:").c_str(), selected.c_str());    
      mqtt.publish((String(name) + "/Seleção:").c_str(), selection.c_str());  

      scale_connected = (xTaskGetTickCount() - scale_lastComm < pdMS_TO_TICKS(1000));

      vTaskDelay(pdMS_TO_TICKS(1000/mqtt_rate));
    }
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