/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////// hub_program.ino ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////// blynk_module.h /////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int sDefaultStep = 40;

WidgetLED xLedSmokeA(V5);
WidgetLED xLedFlameA(V9);
WidgetLED xLedSmokeB(V6);
WidgetLED xLedFlameB(V10);

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

BLYNK_WRITE(V7) {
  xDB1.motor.pwm = param.asInt();
}

BLYNK_WRITE(V8) {
  xDB2.motor.pwm = param.asInt();
}

BLYNK_WRITE(V11) {
  xDB1.motor.mode = param.asInt();
}

BLYNK_WRITE(V12) {
  xDB2.motor.mode = param.asInt();
}

BLYNK_WRITE(V15) {
  xDB1.motor.step = param.asInt();
}

BLYNK_WRITE(V16) {
  xDB2.motor.step = param.asInt();
}

BLYNK_WRITE(V17) {
  if (param.asInt()) {
    xDB1.motor.step = sDefaultStep;
    Blynk.virtualWrite(V15, sDefaultStep);
  }
}

BLYNK_WRITE(V18) {
  if (param.asInt()) {
    xDB2.motor.step = sDefaultStep;
    Blynk.virtualWrite(V16, sDefaultStep);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////// database.h /////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef struct {
  float temperature;
  float humidity;
  int smoke_status;
  int fire_status;
} sensor_t;

typedef struct {
  int mode;
  int pwm;
  int step;
} motor_t;

typedef struct {
  sensor_t sensor;
  motor_t motor;
  int alarm;
} db_t;

static db_t xDB1;
static db_t xDB2;

// Populate default value
void populateDBWithEmptyValue()
{
  xDB1.sensor.temperature = 0;
  xDB1.sensor.humidity = 0;
  xDB1.sensor.smoke_status = 0;
  xDB1.sensor.fire_status = 0;
  xDB1.motor.mode = 0;
  xDB1.motor.pwm = 0;
  xDB1.motor.step = 0;
  xDB1.alarm = 0;

  xDB2.sensor.temperature = 0;
  xDB2.sensor.humidity = 0;
  xDB2.sensor.smoke_status = 0;
  xDB2.sensor.fire_status = 0;
  xDB2.motor.mode = 0;
  xDB2.motor.pwm = 0;
  xDB2.motor.step = 0;
  xDB2.alarm = 0;
}

void vShowDB()
{
#ifdef _DEBUG_
  StaticJsonDocument<384> db;
  
  JsonObject A = db.createNestedObject("A");
  
  JsonObject A_sensor = A.createNestedObject("sensor");
  A_sensor["temperature"] = xDB1.sensor.temperature;
  A_sensor["humidity"] = xDB1.sensor.humidity;
  A_sensor["smoke_status"] = xDB1.sensor.smoke_status;
  A_sensor["fire_status"] = xDB1.sensor.fire_status;
  
  JsonObject A_motor = A.createNestedObject("motor");
  A_motor["mode"] = xDB1.motor.mode;
  A_motor["pwm"] = xDB1.motor.pwm;
  A_motor["step"] = xDB1.motor.step;
  A["alarm"] = xDB1.alarm;
  
  JsonObject B = db.createNestedObject("B");
  
  JsonObject B_sensor = B.createNestedObject("sensor");
  B_sensor["temperature"] = xDB2.sensor.temperature;
  B_sensor["humidity"] = xDB2.sensor.humidity;
  B_sensor["smoke_status"] = xDB2.sensor.smoke_status;
  B_sensor["fire_status"] = xDB2.sensor.fire_status;
  
  JsonObject B_motor = B.createNestedObject("motor");
  B_motor["mode"] = xDB2.motor.mode;
  B_motor["pwm"] = xDB2.motor.pwm;
  B_motor["step"] = xDB2.motor.step;
  B["alarm"] = xDB2.alarm;
  
  serializeJson(db, Serial);
  _PL();
#endif
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////// pitches.h //////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*************************************************
 * Public Constants
 *************************************************/

#define NOTE_B0  31
#define NOTE_C1  33
#define NOTE_CS1 35
#define NOTE_D1  37
#define NOTE_DS1 39
#define NOTE_E1  41
#define NOTE_F1  44
#define NOTE_FS1 46
#define NOTE_G1  49
#define NOTE_GS1 52
#define NOTE_A1  55
#define NOTE_AS1 58
#define NOTE_B1  62
#define NOTE_C2  65
#define NOTE_CS2 69
#define NOTE_D2  73
#define NOTE_DS2 78
#define NOTE_E2  82
#define NOTE_F2  87
#define NOTE_FS2 93
#define NOTE_G2  98
#define NOTE_GS2 104
#define NOTE_A2  110
#define NOTE_AS2 117
#define NOTE_B2  123
#define NOTE_C3  131
#define NOTE_CS3 139
#define NOTE_D3  147
#define NOTE_DS3 156
#define NOTE_E3  165
#define NOTE_F3  175
#define NOTE_FS3 185
#define NOTE_G3  196
#define NOTE_GS3 208
#define NOTE_A3  220
#define NOTE_AS3 233
#define NOTE_B3  247
#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976
#define NOTE_C7  2093
#define NOTE_CS7 2217
#define NOTE_D7  2349
#define NOTE_DS7 2489
#define NOTE_E7  2637
#define NOTE_F7  2794
#define NOTE_FS7 2960
#define NOTE_G7  3136
#define NOTE_GS7 3322
#define NOTE_A7  3520
#define NOTE_AS7 3729
#define NOTE_B7  3951
#define NOTE_C8  4186
#define NOTE_CS8 4435
#define NOTE_D8  4699
#define NOTE_DS8 4978

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// sensor_task.h //////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Add blynk dependencies
#include "blynk_module.h"

void vUpdateSensor()
{
  Blynk.virtualWrite(V1, xDB1.sensor.temperature);
  Blynk.virtualWrite(V2, xDB1.sensor.humidity);
  Blynk.virtualWrite(V13, xDB1.motor.pwm);
  Blynk.virtualWrite(V3, xDB2.sensor.temperature);
  Blynk.virtualWrite(V4, xDB2.sensor.humidity);
  Blynk.virtualWrite(V14, xDB2.motor.pwm);
  xLedSmokeA.setValue(xDB1.sensor.smoke_status == 1 ? 255 : 0);
  xLedFlameA.setValue(xDB1.sensor.fire_status == 1 ? 255 : 0);
  xLedSmokeB.setValue(xDB2.sensor.smoke_status == 1 ? 255 : 0);
  xLedFlameB.setValue(xDB2.sensor.fire_status == 1 ? 255 : 0);

  if (xDB1.alarm == 1 || xDB2.alarm == 1)
  {
    tAlarm.enableIfNot();
  } else {
    noTone(BUZZER_PIN);
    tAlarm.disable();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// server_task.h //////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <AsyncJson.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer xServer(80);

void vMotorController(AsyncWebServerRequest *request) {
  StaticJsonDocument<48> payload;
  String id = request->getParam("id")->value();

  payload["mode"] = (id == "A" ? xDB1.motor.mode : xDB2.motor.mode);
  payload["pwm"] = (id == "A" ? xDB1.motor.pwm : xDB2.motor.pwm);
  payload["step"] = (id == "A" ? xDB1.motor.step : xDB2.motor.step);

  request->send(200, "application/json", payload.as<String>());
}

AsyncCallbackJsonWebHandler* xSensorController = new AsyncCallbackJsonWebHandler("/sensor", [](AsyncWebServerRequest *request, JsonVariant &json) {
  const JsonObject& data = json.as<JsonObject>();
  
  if (data["id"] == "A") {
    xDB1.sensor.temperature = data["temperature"].as<float>();
    xDB1.sensor.humidity = data["humidity"].as<float>();
    xDB1.sensor.smoke_status = data["smoke_status"].as<int>();
    xDB1.sensor.fire_status = data["fire_status"].as<int>();
    xDB1.motor.pwm = data["pwm"].as<int>();
    xDB1.alarm = data["alarm"].as<int>();
  } else {
    xDB2.sensor.temperature = data["temperature"].as<float>();
    xDB2.sensor.humidity = data["humidity"].as<float>();
    xDB2.sensor.smoke_status = data["smoke_status"].as<int>();
    xDB2.sensor.fire_status = data["fire_status"].as<int>();
    xDB2.motor.pwm = data["pwm"].as<int>();
    xDB2.alarm = data["alarm"].as<int>();
  }
  
  request->send(200);
});

void vSetupServer() {
  xServer.on("/motor", HTTP_GET, vMotorController);
  xServer.addHandler(xSensorController);
  xServer.begin();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////// telegram_task.h ////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const char* token = "5593489325:AAHkoog4XIb2hLRbuLtPXWIjIORKMjrfuw4";
const char* channel = "@pusat_notifikasi";
String message = "Terdapat kebakaran!!";

void vSetupTelegramBot() {
  bot.setClock("WIB-7");
  bot.setUpdateTime(500);
  bot.setTelegramToken(token);

  _PL();
  _PL("Test Telegram connection... ");
  bot.begin() ? _PL("OK") : _PL("NOK");
}

void vSendTelegram() {
  if (xDB1.alarm == 1 || xDB2.alarm == 1)
    bot.sendToChannel(channel, message, true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////// wifi_connection.h ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>

const char * AP_SSID = "Tugas Akhir";
const char * AP_PASS = "tug454kh1r";
const char * STA_SSID = "Enggar";
const char * STA_PASS = "drenggarbaik";

WiFiEventHandler wifiDisconnectHandler;

uint8_t cInitWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.begin(STA_SSID, STA_PASS);

  return WiFi.waitForConnectResult();
}

void vSetupWiFi() {
  wifiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    WiFi.begin(STA_SSID, STA_PASS);
    
    uint8_t cRet = WiFi.waitForConnectResult();
    if (cRet != WL_CONNECTED) {
      _PL("WiFi Failed");
      delay(2 * TASK_SECOND);
      ESP.restart();
    }
  });

  uint8_t cRet = cInitWiFi();
  if (cRet != WL_CONNECTED) {
    _PL("WiFi Failed");
    delay(2 * TASK_SECOND);
    ESP.restart();
  }

  _PP("Station IP = "); _PL(WiFi.localIP());
  _PP("Access Point IP = "); _PL(WiFi.softAPIP());
}
