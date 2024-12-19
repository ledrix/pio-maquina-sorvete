#include "pcb.h"
#include "config.h"



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

  digitalWrite(LED,HIGH);
  delay(500);
  digitalWrite(LED,LOW);
  digitalWrite(SPINNER,HIGH);
  delay(500);
  digitalWrite(SPINNER,LOW);
  digitalWrite(MIXER,HIGH);
  delay(500);
  digitalWrite(MIXER,LOW);

  Serial.begin(115200);

}

void loop() 
{
  digitalWrite(LED, !digitalRead(LEVEL));
  digitalWrite(MIXER, !digitalRead(RUNNING));
  digitalWrite(SPINNER, !digitalRead(PRESSURE));

  Serial.printf("\nLevel --> %d\n", !digitalRead(LEVEL));
  Serial.printf("Current --> %d\n", !digitalRead(RUNNING));
  Serial.printf("Pressure --> %d\n", !digitalRead(PRESSURE));
  Serial.printf("Temperature --> %d\n", !digitalRead(TEMPERATURE));

  //Cap. was not enough to sustain input state throughout the sine wave, 
  //needs additional logic to handle it
  
  // delay(1000);
}