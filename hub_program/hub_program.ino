// Comment to disable blynk serial print
//#define BLYNK_PRINT Serial

#include <Arduino.h>
#include <ArduinoJson.h>
#include <TaskScheduler.h>
//#include <ESP8266mDNS.h>
#include <BlynkSimpleEsp8266.h>

#include "AsyncTelegram.h"
AsyncTelegram bot;

char sBlynkAuth[] = "33zZWYGjoQNzCwZ7bpTdNpU-JhollxJ3";
char sBlynkServer[] = "18.141.174.184";
int sBlynkPort = 8080;

// Comment to disable debug mode
#define _DEBUG_
#ifdef _DEBUG_
#define _PP(ARG) Serial.print(ARG)
#define _PL(ARG) Serial.println(ARG)
#define _PF(...) Serial.printf(__VA_ARGS__)
#else
#define _PP(ARG)
#define _PL(ARG)
#define _PF(...)
#endif

#define BUZZER_PIN 12

// Simple database
#include "database.h"

// Forward declaration
void vA5Pitch();
void vA6Pitch();
void vUpdateSensor();
void vSendTelegram();

Scheduler ts;

Task tSensor(500 * TASK_MILLISECOND, TASK_FOREVER, &vUpdateSensor, &ts);
Task tAlarm(500 * TASK_MILLISECOND, TASK_FOREVER, &vA5Pitch, &ts);
Task tDB(2 * TASK_SECOND, TASK_FOREVER, &vShowDB, &ts);
Task tBot(2 * TASK_SECOND, TASK_FOREVER, &vSendTelegram, &ts);

#include "wifi_connection.h"
#include "telegram_task.h"
#include "sensor_task.h"
#include "server_task.h"
#include "pitches.h"

void setup() {
#ifdef _DEBUG_
  Serial.begin(115200);
#endif
  _PL();
  _PL("Booting...");

  populateDBWithEmptyValue();
  vSetupWiFi();
  vSetupServer();
  vSetupTelegramBot();

//  MDNS.begin(host);
//  MDNS.addService("http", "tcp", 80);
  Blynk.config(sBlynkAuth, sBlynkServer, sBlynkPort);
  Blynk.connect();

  tSensor.enable();
  tDB.enable();
  tBot.enable();

//  _PF("Ready! Open http://%s.local in your browser\n", host);
  _PL("Booting success");
}

void loop() {
  ts.execute();
  Blynk.run();
  
  TBMessage msg;
  bot.getNewMessage(msg);
  
  delay(2);
}

void vA5Pitch() {
  tone(BUZZER_PIN, NOTE_A5);
  tAlarm.setCallback(&vA6Pitch);
}

void vA6Pitch() {
  tone(BUZZER_PIN, NOTE_A6);
  tAlarm.setCallback(&vA5Pitch);
}
