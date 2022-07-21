/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////// node_program.ino //////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DEVICE_ID "A"
//#define DEVICE_ID "B"

#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <Adafruit_SHTC3.h>

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

#define MQ2_PIN A0

// For sub A
//#define MOTOR_PIN 12
//#define FIRE_PIN 14

// For sub B
#define MOTOR_PIN 14
#define FIRE_PIN 12

// Globals
typedef struct {
  float temperature;
  float humidity;
  int smoke_status;
  int fire_status;
} sensor_struct;

typedef struct {
  int mode;
  int pwm;
} motor_t;

typedef struct {
  sensor_struct sensor;
  motor_t motor;
  int alarm;
} db_t;

db_t xDB = { {0, 0, 0, 0}, {0, 0}, 0 };

bool cIsControlAuto = false;

// PD realated variable
float sSetPoint = 40.00;
float sKp = 11.378;
float sTd = 11.5455;
float sLastError = 0;

const char * pcSensorEndpoint = "http://192.168.4.1/sensor";
//const char * pcMotorEndpoint = "http://192.168.4.1/motor?id=A";
const char * pcMotorEndpoint = "http://192.168.4.1/motor?id=B";

Adafruit_SHTC3 xSHTC3 = Adafruit_SHTC3();
sensors_event_t xHumidity, xTemperature;

Scheduler runner;

// Forward declaration
void vSHTC3Routine();
void vSensorTask();
void vControlTask();
void vMotorTask();
void vPIDTask();

Task tSHTC3(500 * TASK_MILLISECOND, TASK_FOREVER, &vSHTC3Routine, &runner);
Task tSensor(1 * TASK_SECOND, TASK_FOREVER, &vSensorTask, &runner);
Task tControl(500 * TASK_MILLISECOND, TASK_FOREVER, &vControlTask, &runner);
Task tMotor(200 * TASK_MILLISECOND, TASK_FOREVER, &vMotorTask, &runner);
Task tPID(500 * TASK_MILLISECOND, TASK_FOREVER, &vPIDTask, &runner);

#include "api_module.h"
#include "sensor_module.h"
#include "control_module.h"
#include "motor_module.h"
#include "wifi_connection.h"

void setup() {
#ifdef _DEBUG_
  Serial.begin(115200);
#endif
  _PL();
  _PL("Booting...");

  pinMode(FIRE_PIN, INPUT_PULLUP);
  analogWriteResolution(10);
  xSHTC3.begin();
  vSetupWiFi();

  // Heating process
  _PL("Heating MQ2 for 30 seconds...");
  delay(30 * TASK_SECOND);
  _PL("Done heating");

  tSHTC3.enable();
  tControl.enable();
  tSensor.enable();
  tMotor.enable();

  _PL("Booting success");
}

void loop() {
  runner.execute();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////// api_module.h ////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>

void vSendApiPost(const char * endpoint, void * param) {
  StaticJsonDocument<128> payload;
  db_t * data = (db_t *) param;
  
  WiFiClient client;
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.useHTTP10(true);
  http.begin(client, endpoint);

  // Specify content-type header
  http.addHeader("Content-Type", "application/json");
  
  // Data to send with HTTP POST
  payload["id"] = DEVICE_ID;
  payload["temperature"] = data->sensor.temperature;
  payload["humidity"] = data->sensor.humidity;
  payload["smoke_status"] = data->sensor.smoke_status;
  payload["fire_status"] = data->sensor.fire_status;
  payload["pwm"] = data->motor.pwm;
  payload["alarm"] = data->alarm;
  
  // Send HTTP POST request
#ifdef _DEBUG_
  serializeJson(payload, Serial);
  _PL();
#endif
  int httpResponseCode = http.POST(payload.as<String>());

  // Free resources
  http.end();
}

void vSendApiGet(const char * endpoint) {
  StaticJsonDocument<96> data;
  WiFiClient client;
  HTTPClient http;
  
  // Your Domain name with URL path or IP address with path
  http.useHTTP10(true);
  http.begin(client, endpoint);
  
  // Send HTTP GET request
  int httpResponseCode = http.GET();
  
  deserializeJson(data, http.getStream());
#ifdef _DEBUG_
  serializeJson(data, Serial);
  _PL();
#endif

  xDB.motor.mode = data["mode"].as<int>();
  xDB.motor.pwm = (xDB.motor.mode == 0 ? data["pwm"].as<int>() : xDB.motor.pwm);
  sSetPoint = data["step"].as<int>();
  
  // Free resources
  http.end();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////// control_module.h //////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void vControlTask() {
  if (xDB.sensor.temperature >= sSetPoint && xDB.sensor.smoke_status == HIGH && xDB.sensor.fire_status == HIGH)
    xDB.alarm = 1;
  else if (xDB.sensor.temperature >= sSetPoint && xDB.sensor.smoke_status == HIGH && xDB.sensor.fire_status == LOW)
    xDB.alarm = 1;
  else if (xDB.sensor.temperature >= sSetPoint && xDB.sensor.smoke_status == LOW && xDB.sensor.fire_status == HIGH)
    xDB.alarm = 1;
  else if (xDB.sensor.temperature < sSetPoint && xDB.sensor.smoke_status == HIGH && xDB.sensor.fire_status == HIGH)
    xDB.alarm = 1;
  else if (tPID.isEnabled())
    xDB.alarm = 1;
  else xDB.alarm = 0;

  if (cIsControlAuto) {
    if (xDB.alarm == 1 && xDB.sensor.temperature >= sSetPoint)
      tPID.enableIfNot();
    else if (tPID.isEnabled() && xDB.sensor.temperature < sSetPoint) {
      tPID.disable();
      analogWrite(MOTOR_PIN, 0);
      xDB.motor.pwm = 0;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////// motor_module.h ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void vMotorTask() {
  vSendApiGet(pcMotorEndpoint);
  cIsControlAuto = (xDB.motor.mode == 1 ? true : false); // Update after get response
  
  if (!cIsControlAuto) {
    if (tPID.isEnabled()) {
      tPID.disable();
    }
    analogWrite(MOTOR_PIN, xDB.motor.pwm);
  } else if (!tPID.isEnabled()) {
    analogWrite(MOTOR_PIN, 0);
    xDB.motor.pwm = 0;
  }
}

void vPIDTask() {
  float sOutput = 0;
  float sInput = xTemperature.temperature;
  float sError = sInput - sSetPoint;
  float sDeltaError = sError - sLastError;

  sOutput = sOutput + (sKp * sError);
  sOutput = sOutput + (sKp * sTd * sDeltaError);
  sLastError = sError;

  if (sOutput < 0) sOutput = 0;
  else if (sOutput > 255) sOutput = 255;
  
  sOutput = map(sOutput, 0, 255, 800, 1023);
  analogWrite(MOTOR_PIN, sOutput);

  xDB.motor.pwm = sOutput;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////// sensor_module.h //////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void vSHTC3Routine() {
  // Populate temp and humidity objects with fresh data
  xSHTC3.getEvent(&xHumidity, &xTemperature);
  
  xDB.sensor.temperature = xTemperature.temperature;
  xDB.sensor.humidity = xHumidity.relative_humidity;
}

void vSensorTask() {
  xDB.sensor.smoke_status = (analogRead(MQ2_PIN) < 450 ? HIGH : LOW);
  xDB.sensor.fire_status = digitalRead(FIRE_PIN);
  
  vSendApiPost(pcSensorEndpoint, (void *) &xDB);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////// wifi_connection.h /////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>

const char * pcSSID = "Tugas Akhir";
const char * pcPassword = "tug454kh1r";

WiFiEventHandler xWiFiDisconnectHandler;

uint8_t cInitWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(pcSSID, pcPassword);

  return WiFi.waitForConnectResult();
}

void vSetupWiFi() {
  xWiFiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    WiFi.begin(pcSSID, pcPassword);
    
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
}
