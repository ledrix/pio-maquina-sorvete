#include "pcb.h"
#include "config.h"
#include "version.h"

volatile bool state[] = { false, 
                          false, 
                          false, 
                          false };

volatile bool level = false;
volatile bool running = false;
volatile bool pressure = false;
volatile bool temperature = false;

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

}

void loop() 
{
  digitalWrite(LED, level);
  digitalWrite(MIXER, running);
  digitalWrite(SPINNER, pressure);
}