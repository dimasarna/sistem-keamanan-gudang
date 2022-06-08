#define APPVER "1.0.0"
#define APPTOKEN "FIREALARM"
#define EEPROM_MAX 4096

#define _DEBUG_
//#define _TEST_

#ifdef _DEBUG_
#define _PP(a) Serial.print(a);
#define _PL(a) Serial.println(a);
#else
#define _PP(a)
#define _PL(a)
#endif

#ifdef _DEBUG_
#define APPPREFIX "firealarmtest-"
#else
#define APPPREFIX "firealarm-"
#endif
#define APPHOST "192.168.216.13"
#define APPPORT 80
#define APPURL "/esp/esp32.php"

#include <DHT.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <JsonConfigHttp.h>
#include <EspBootstrapDict.h>
#include <ParametersEEPROM.h>

#include "esp_bt.h"
#include <DHT_U.h>
#include <HTTPUpdate.h>
#include <TaskScheduler.h>
#include <Adafruit_Sensor.h>
#include <BlynkSimpleEsp32.h>
#include <UniversalTelegramBot.h>

String appVersion;

const char SSID1[] = "POCO X3 NFC";
const char PWD1[] = "iyasebentar";

const String TOKEN(APPTOKEN);
const int NPARS = 13;
const int NPARS_WEB = 3;

Dictionary* pd_ptr = NULL;
ParametersEEPROM* pp_ptr = NULL;

#define DHTPIN_DEPAN 18     // Digital pin connected to the DHT sensor
#define DHTPIN_BELAKANG 26     // Digital pin connected to the DHT sensor
#define MQ2PIN_DEPAN 35     // ADC pin connected to MQ2 sensor
#define MQ2PIN_BELAKANG 33     // ADC pin connected to MQ2 sensor

// Uncomment the type of sensor in use:
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

DHT_Unified dhtDepan(DHTPIN_DEPAN, DHTTYPE);
DHT_Unified dhtBelakang(DHTPIN_BELAKANG, DHTTYPE);

// Untuk ruangan depan
float suhuDepan = 0;
float kelembapanDepan = 0;
uint32_t asapDepan = 0;

// Untuk ruangan belakang
float suhuBelakang = 0;
float kelembapanBelakang = 0;
uint32_t asapBelakang = 0;

// Smoke threshold value
uint32_t smokeThreshold = 350;

// Set this flag to false to indicate hazard condition
// and true to indicate normal condition
bool NORMAL_CONDITION = true;

// Proportional constant
//float pConstant = 0;

// Derivative constant
//float dConstant = 0;

// Set delay between sensor readings based on sensor datasheet.
uint32_t monitoringDelay = 2; // 0.5 Hz

// Delay between controlling action
uint32_t controllingDelay = 10;

void vMonitorTask();
void vControlTask();
void vBlynkTask();

Scheduler runner;

Task tMonitoring(2 * TASK_SECOND, TASK_FOREVER, &vMonitorTask, &runner);
Task tControlling(10 * TASK_SECOND, TASK_FOREVER, &vControlTask, &runner);
Task tBlynk(50 * TASK_MILLISECOND, TASK_FOREVER, &vBlynkTask, &runner);

String blynk_auth,
          blynk_host,
          bot_token,
          chat_id;

WiFiClientSecure secured_client;
UniversalTelegramBot bot(bot_token, secured_client);

// Error handler prototype
void fatalError(String message);

void setup() {
// Start serial connection in debug mode
#ifdef _DEBUG_
  Serial.begin(115200); delay(1 * TASK_SECOND);
  _PL("Setting up application...");
#endif

  setupOTA();
  setupParameters();
  setupBT();
  checkOTA();
  setupBlynk();

  // Add root certificate for api.telegram.org
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  
  // Set bot config
  Dictionary& pd = *pd_ptr;
  bot_token = String(pd["notif_token"]);
  chat_id = String(pd["chat_id"]);
  bot.updateToken(bot_token);

  // Initialize device.
  dhtDepan.begin();
  dhtBelakang.begin();

  // Wait for 20 seconds to stabilize sensor
//  _PL("Please wait for stabilizing sensor...");
//  
//  delay(20 * TASK_SECOND);
//  
//  _PL("Done.");

  // Untuk ruangan depan    
  
  /**
   * @brief Setup pwm channel and frequency
   * @param chan
   * @param freq
   * @param bit_num
   */
  ledcSetup(14, 5000, 8);
  
  /**
   * @brief Attach pwm to GPIO pin
   * @param pin
   * @param chan
   */
  ledcAttachPin(22, 14);

  // Untuk ruangan belakang
  
  /**
   * @brief Setup pwm channel and frequency
   * @param chan
   * @param freq
   * @param bit_num
   */
  ledcSetup(15, 5000, 8);
  
  /**
   * @brief Attach pwm to GPIO pin
   * @param pin
   * @param chan
   */
  ledcAttachPin(21, 15);

  // Setup for buzzer operation
  
  /**
   * @brief Setup pwm channel and frequency
   * @param chan
   * @param freq
   * @param bit_num
   */
  ledcSetup(13, 5000, 8);
  
  /**
   * @brief Attach pwm to GPIO pin
   * @param pin
   * @param chan
   */
  ledcAttachPin(5, 13);

  tMonitoring.enable();
  tControlling.enable();
  tBlynk.enable();
}

void loop() {
  // put your main code here, to run repeatedly:
  runner.execute();
}

void vMonitorTask()
{    
    // Get temperature event and store its value.
    sensors_event_t kondisiDepan;
    dhtDepan.temperature().getEvent(&kondisiDepan);
    if (!isnan(kondisiDepan.temperature)) {
      suhuDepan = kondisiDepan.temperature;
      Blynk.virtualWrite(V1, suhuDepan);
    }
    
    // Get humidity event and print its value.
    dhtDepan.humidity().getEvent(&kondisiDepan);
    if (!isnan(kondisiDepan.relative_humidity)) {
      kelembapanDepan = kondisiDepan.relative_humidity;
      Blynk.virtualWrite(V2, kelembapanDepan);
    }

    // Get temperature event and store its value.
    sensors_event_t kondisiBelakang;
    dhtBelakang.temperature().getEvent(&kondisiBelakang);
    if (!isnan(kondisiBelakang.temperature)) {
      suhuBelakang = kondisiBelakang.temperature;
      Blynk.virtualWrite(V3, suhuBelakang);
    }
    
    // Get humidity event and print its value.
    dhtBelakang.humidity().getEvent(&kondisiBelakang);
    if (!isnan(kondisiBelakang.relative_humidity)) {
      kelembapanBelakang = kondisiBelakang.relative_humidity;
      Blynk.virtualWrite(V4, kelembapanBelakang);
    }
    
    // Get analog value from mq2 sensor
    asapDepan = analogRead(MQ2PIN_DEPAN);
    asapBelakang = analogRead(MQ2PIN_BELAKANG);
    Blynk.virtualWrite(V5, asapDepan);
    Blynk.virtualWrite(V6, asapBelakang);

    _PP("Ruang Depan\t"); _PP(suhuDepan); _PP("°C\t"); _PP(kelembapanDepan); _PP("%\t"); _PL(asapDepan);
    _PP("Ruang Belakang\t"); _PP(suhuBelakang); _PP("°C\t"); _PP(kelembapanBelakang); _PP("%\t"); _PL(asapBelakang);
}

void vControlTask()
{
    if ( suhuDepan > 35.0 && asapDepan > smokeThreshold ) {
      // Calculate error relative to target temperature (lower than 40)
      // maximum error is 20 because dht11 have range between 0 and 60 degree
//      uint32_t tempError = suhuDepan - 40;
      
//      float pValue = pConstant * (float)map(suhuDepan, 40, 60, 0, 128);
//      float dValue = dConstant * (float)map(tempError, 0, 20, 0, 64);

      // Calculate duty cycle, 127 is lowest duty cycle which is equal to 50% of motor voltage
//      uint32_t dutyCycle = round(127 + pValue + dValue);

      /**
       * @brief Set duty cycle of pwm channel
       * @param chan
       * @param duty
       */
//      ledcWrite(14, dutyCycle);
      ledcWrite(14, 255);
      
      // Turn on alarm
      ledcWrite(13, 128);

      // Send message to bot
      if (NORMAL_CONDITION)
        bot.sendMessage(chat_id, "Terdapat Kebakaran!", "");

      // Set hazard flag
      NORMAL_CONDITION = false;
    } else {
      ledcWrite(14, 0);
      NORMAL_CONDITION = true;
    }

    if ( suhuBelakang > 35.0 && asapBelakang > smokeThreshold ) {
      // Calculate error relative to target temperature (lower than 40)
      // maximum error is 20 because dht11 have range between 0 and 60 degree
//      uint32_t tempError = suhuBelakang - 40;
      
//      float pValue = pConstant * (float)map(suhuBelakang, 40, 60, 0, 128);
//      float dValue = dConstant * (float)map(tempError, 0, 20, 0, 64);

      // Calculate duty cycle, 127 is lowest duty cycle which is equal to 50% of motor voltage
//      uint32_t dutyCycle = round(127 + pValue + dValue);

      /**
       * @brief Set duty cycle of pwm channel
       * @param chan
       * @param duty
       */
//      ledcWrite(15, dutyCycle);
      ledcWrite(15, 255);

      // Turn on alarm
      ledcWrite(13, 128);

      // Send message to bot
      if (NORMAL_CONDITION)
        bot.sendMessage(chat_id, "Terdapat Kebakaran!", "");

      // Set hazard flag
      NORMAL_CONDITION = false;
    } else {
      ledcWrite(15, 0);
      NORMAL_CONDITION = true;
    }

    if (NORMAL_CONDITION) {
      // Turn off alarm
      ledcWrite(13, 0);
    }
}

void vBlynkTask() {
  Blynk.run();
}

void setupParameters() {
  int rc;
  bool wifiTimeout;

  pd_ptr = new Dictionary(NPARS);
  Dictionary& pd = *pd_ptr;

  pp_ptr = new ParametersEEPROM(TOKEN, pd, 0, 512);
  ParametersEEPROM& pp = *pp_ptr;

  if (pp.begin() != PARAMS_OK) {
    // Something is wrong with the EEPROM
    fatalError("Something is wrong with the EEPROM");
  }

  pd("Title", "Fire Alarm System");
  pd("ssid", SSID1);
  pd("password", PWD1);
  pd("cfg_url", "http://192.168.216.13/esp/config/");
  pd("ota_host", APPHOST);
  pd("ota_port", APPPORT);
  pd("ota_url", APPURL);

  rc = pp.load();

  _PL("Connecting to WiFi for 20 sec:");
  setupWifi(pd["ssid"].c_str(), pd["password"].c_str());
  wifiTimeout = waitForWifi(20 * BOOTSTRAP_SECOND);

  if (!wifiTimeout) {
    _PL(makeConfig(pd["cfg_url"]));
    rc = JSONConfig.parse(makeConfig(pd["cfg_url"]), pd);
    
    // If successful, the "pd" dictionary should have a refreshed set of parameters from the JSON file.
    _PP("JSONConfig finished. rc = "); _PL(rc);
    _PP("Current dictionary count = "); _PL(pd.count());
    _PP("Current dictionary size = "); _PL(pd.size());

    if (rc == JSON_OK) pd("saved", "ok");
  }
  if (wifiTimeout || !(rc == JSON_OK || pd("saved"))) {
    _PL("Device needs bootstrapping:");
    rc = ESPBootstrap.run(pd, NPARS_WEB, 10 * BOOTSTRAP_MINUTE);

    if (rc == BOOTSTRAP_OK) {
      pp.save();
      _PL("Bootstrapped OK. Rebooting.");
    }
    else {
      _PL("Bootstrap timed out. Rebooting.");
    }
    
    delay(2 * TASK_SECOND);
    ESP.restart();
  }
}

// This method prepares for WiFi connection
void setupWifi(const char* ssid, const char* pwd) {
  // We start by connecting to a WiFi network
  _PL("Connecting to WiFi...");
  // clear wifi config
  WiFi.disconnect();

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pwd);
}

// This method waits for a WiFi connection for aTimeout milliseconds.
bool waitForWifi(unsigned long aTimeout) {
  unsigned long timeNow = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    _PP(".");
    if (millis() - timeNow > aTimeout) {
      _PL(" WiFi connection timeout");
      return true;
    }
  }

  _PL(" WiFi connected");
  _PP("IP address: "); _PL(WiFi.localIP());
  _PP("SSID: "); _PL(WiFi.SSID());
  _PP("mac: "); _PL(WiFi.macAddress());
  
  delay(2 * TASK_SECOND); // let things settle
  return false;
}

void setupBT() {
  esp_bt_controller_disable();
}

String makeConfig(String path) {
  String cfg(path);
  if (!cfg.endsWith("/")) cfg += "/";
  cfg += (appVersion + ".json");
  return cfg;
}

void setupOTA() {
  appVersion = String(APPPREFIX) + WiFi.macAddress() + String("-") + String(APPVER);
  appVersion.replace(":", "");
  appVersion.toLowerCase();
}

void checkOTA() {
  Dictionary& pd = *pd_ptr;
  WiFiClient espClient;

  _PL("Attempting OTA");
  _PP("host: "); _PL(pd["ota_host"]);
  _PP("port: "); _PL(pd["ota_port"]);
  _PP("url : "); _PL(pd["ota_url"]);
  _PP("ver : "); _PL(appVersion);

//  httpUpdate.setLedPin(LEDPIN, HIGH);
  t_httpUpdate_return ret = httpUpdate.update(espClient, pd["ota_host"], pd["ota_port"].toInt(), pd["ota_url"], appVersion);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      _PP("HTTP_UPDATE_FAILED: Error code=");
      _PP(httpUpdate.getLastError());
      _PP(" ");
      _PL(httpUpdate.getLastErrorString());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      _PL("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      _PL("HTTP_UPDATE_OK");
      break;
  }
}

void setupBlynk() {
  _PL("Setup connection with blynk");

  Dictionary& pd = *pd_ptr;
  _PL("Blynk parameters:");
  _PP("blynk_auth: "); _PL(pd["blynk_auth"].c_str());
  _PP("blynk_host: "); _PL(pd["blynk_host"].c_str());
  _PP("blynk_port: "); _PL(pd["blynk_port"].toInt());

  blynk_auth = String(pd["blynk_auth"]);
  blynk_host = String(pd["blynk_host"]);
  Blynk.config(blynk_auth.c_str(), blynk_host.c_str(), pd["blynk_port"].toInt());
}

void fatalError(String message) {
  // Print error message
  _PL(message);
  
  // Wait for ten seconds
  delay(10 * TASK_SECOND);

  // Software restart
  ESP.restart();
}
