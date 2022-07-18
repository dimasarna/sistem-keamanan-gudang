#define DEVICE_ID "A"

#include <ArduinoJson.h>
#include <TaskScheduler.h>
#include <Adafruit_SHTC3.h>
//#include <ESP8266mDNS.h>

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
#define MOTOR_PIN 12
#define FIRE_PIN 14

// For sub B
//#define MOTOR_PIN 14
//#define FIRE_PIN 12

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
float sKp = 5.689;
float sTd = 11.5455;
float sLastError = 0;

//const char* pcHost = "esp8266a-sub";
//const char* pcHost = "esp8266b-sub";
const char * pcSensorEndpoint = "http://192.168.4.1/sensor";
const char * pcMotorEndpoint = "http://192.168.4.1/motor?id=A";
//const char * pcMotorEndpoint = "http://192.168.4.1/motor?id=B";

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
Task tControl(500 * TASK_SECOND, TASK_FOREVER, &vControlTask, &runner);
Task tMotor(200 * TASK_MILLISECOND, TASK_FOREVER, &vMotorTask, &runner);
Task tPID(500 * TASK_MILLISECOND, TASK_FOREVER, &vPIDTask, &runner);

#include "api_module.h"
#include "sensor_module.h"
#include "control_module.h"
#include "motor_module.h"
#include "wifi_connection.h"

void setup() {
  pinMode(FIRE_PIN, INPUT_PULLUP);
#ifdef _DEBUG_
  Serial.begin(115200);
#endif
  _PL();
  _PL("Booting...");

  vSetupWiFi();

//  MDNS.begin(host);
//  MDNS.addService("http", "tcp", 80);
  
  xSHTC3.begin();

  // Heating process
  _PL("Heating MQ2 for 30 seconds...");
  delay(30 * TASK_SECOND);
  _PL("Done heating");

  tSHTC3.enable();
  tControl.enable();
  tSensor.enable();
  tMotor.enable();

//  _PF("Ready! Open http://%s.local in your browser\n", host);
  _PL("Booting success");
}

void loop() {
  runner.execute();
}
