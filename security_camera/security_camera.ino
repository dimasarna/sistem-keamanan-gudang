#define APP_CPU 1
#define PRO_CPU 0
#define APPVER "1.0.0"
#define APPTOKEN "SECURECAM"
#define EEPROM_MAX 4096
#define APPHOST "54.67.18.187"
#define APPPORT 80
#define APPURL "/esp/esp32.php"

#define _DEBUG_
//#define _TEST_

#ifdef _DEBUG_
#define _PP(a) Serial.print(a);
#define _PL(a) Serial.println(a);
#define _PF(...) Serial.printf(__VA_ARGS__);
#else
#define _PP(a)
#define _PL(a)
#define _PF(...)
#endif
#ifdef _DEBUG_
#define APPPREFIX "securitycameratest-"
#else
#define APPPREFIX "securitycamera-"
#endif

#include <WiFi.h>
#include <WiFiClient.h>
#include <ParametersEEPROM.h>
#include <EspBootstrapDict.h>
#include <JsonConfigHttp.h>
#include <ESP32Time.h>
#include <HTTPUpdate.h>

//#include <mdns.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "camera_config.h"
#include "FS.h" // SD Card ESP32
#include "SD_MMC.h" // SD Card ESP32

String CURRENT_VERSION;

const byte LEDPIN = 33, FLASHPIN = 4;

// GPIO3 connected to the PIR sensor.
const byte SENSORPIN = 3;

// GPIO12 connected to buzzer.
const byte BUZZERPIN = 12;

// PWM config for alarm buzzer
uint32_t freq = 2000;
uint8_t channel = 15, resolutionBits = 8;

const char SSID1[] = "Enggar";
const char PWD1[] = "drenggarbaik";

// Configuration for bootstrap
const String TOKEN(APPTOKEN);
const byte NPARS = 8;
const byte NPARS_WEB = 3;
Dictionary* pd_ptr = NULL;
ParametersEEPROM* pp_ptr = NULL;

// Configuration for streaming service
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
httpd_handle_t stream_httpd = NULL;

// For handle time
ESP32Time rtc;

// RTOS task handle
TaskHandle_t xBlynkTaskHandle,
                     xSensorTaskHandle,
                     xRecordTaskHandle,
                     xMessageBotTaskHandle,
                     xFileBotTaskHandle;

/**
 * Because of we need to send data to two bot, file bot and message bot,
 * and there is just one ssl_client, we need to use mutex to handle queue when
 * two bot need to sending data, only one bot at time is allowable.
 */
SemaphoreHandle_t xMutex;

// Declare RTOS queue
QueueHandle_t xMessageQueue, xFileQueue;

// For wrap file details before sending to queue
typedef struct FileMessage
{
  char fileName[40];
  unsigned long fileSize;
} FileMessage;

#include "error_handler.h"
#include "avi_program.h"
#include "rtos_program.h"

void setup() {
  // Setup led pin
  pinMode(LEDPIN, OUTPUT);

  // Turn on led to indicate booting process
  digitalWrite(LEDPIN, LOW);

  // If debuf mode active, begin serial communication
#ifdef _DEBUG_
  Serial.begin(115200);
  delay(2000);
  _PL();
  _PL("> Setup application");
  _PP("> Total PSRAM: "); _PL(ESP.getPsramSize());
  _PP("> Free PSRAM: "); _PL(ESP.getFreePsram());
#endif

  // Setup for over the air update
  setup_ota();

  // Run bootstrap program
  setup_parameters();

  // Actually checking for update to server
  check_update();

  // Disable bluetooth because we dont needed
  disable_BT();

  // Check if sdcard available, restart if no sdcard detected
  sdcard_init();
  
  // Setup buzzer
  ledcSetup(channel, freq, resolutionBits);
  ledcAttachPin(BUZZERPIN, channel);
  
  // Check camera health, restart if error occured
  if (camera_init() == ESP_OK) {
    _PL("OK!");
  } else {
    fatalError();
  }

  // Get time, restart if error occured
  time_init();

  // Start mDNS service
//  start_mdns();

  // Start streaming server
  start_server();

  // Wait for 15 seconds to stabilize PIR sensor
  _PL("> Waiting for 15 seconds to stabilize PIR sensor");
  delay(15000);
  _PL("OK!");
  
  // Create and run task
  create_tasks();
  
  // Delete setup and loop task
  vTaskDelete(NULL);
}

void loop() {}

// Helper function to make config uri
String makeConfig(String path) {
  String cfg(path);
  if (!cfg.endsWith("/")) cfg += "/";
  cfg += (CURRENT_VERSION + ".json");
  return cfg;
}

void disable_BT() { esp_bt_controller_disable(); }

void setupWifi(const char* ssid, const char* pwd) {
  _PP("Connecting to WiFi ");
  
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pwd);
}

bool waitForWifi(unsigned long aTimeout) {
  unsigned long timeNow = millis();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    _PP(".");
    if (millis() - timeNow > aTimeout) {
      _PL(" WiFi connection timeout!");
      return true;
    }
  }

  _PL(" WiFi connected.");
  _PP("IP address: "); _PL(WiFi.localIP());
  _PP("SSID: "); _PL(WiFi.SSID());
  _PP("MAC: "); _PL(WiFi.macAddress());
  
  delay(2000);
  return false;
}

void setup_parameters() {
  int rc;
  bool wifiTimeout;

  pd_ptr = new Dictionary(NPARS);
  Dictionary& pd = *pd_ptr;

  pp_ptr = new ParametersEEPROM(TOKEN, pd, 0, 512);
  ParametersEEPROM& pp = *pp_ptr;

  _PL("> Start EEPROM");
  if (pp.begin() != PARAMS_OK) {
    fatalError();
  } else {
    _PL("OK!");
  }

  pd("Title", "Security Camera Initial Config");
  pd("ssid", SSID1);
  pd("pwd", PWD1);
  pd("cfg_url", "http://54.67.18.187/esp/config/");

  rc = pp.load();
  _PL("> Connecting to WiFi for 20 sec:");
  setupWifi(pd["ssid"].c_str(), pd["pwd"].c_str());
  wifiTimeout = waitForWifi(20 * BOOTSTRAP_SECOND);

  bool configSaved = false;
  if (!wifiTimeout) {
    _PL(makeConfig(pd["cfg_url"]));
    rc = JSONConfig.parse(makeConfig(pd["cfg_url"]), pd);
    
    _PP("> JSONConfig finished with response code = "); _PL(rc);
    _PP("Current dictionary count = "); _PL(pd.count());
    _PP("Current dictionary size = "); _PL(pd.size());

    if (rc == JSON_OK) configSaved = true;
  }
  if (wifiTimeout || !(rc == JSON_OK || configSaved)) {
    _PL("> Device needs bootstrapping!");
    rc = ESPBootstrap.run(pd, NPARS_WEB, 10 * BOOTSTRAP_MINUTE);

    if (rc == BOOTSTRAP_OK) {
      pp.save();
      _PL("> Bootstrapped OK. Rebooting.");
    }
    else {
      _PL("> Bootstrap timed out. Rebooting.");
    }
    delay(2000);
    ESP.restart();
  }
}

void sdcard_init()
{
  _PL("> Initialising SD card");
 
  if (!SD_MMC.begin("/sdcard", true)) {
    _PL("SD Card Mount Failed");
    fatalError();
  }
  
  // Turn off flash
  pinMode(FLASHPIN, OUTPUT);
  digitalWrite(FLASHPIN, LOW);
  
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    _PL("No SD Card attached");
    fatalError();
  } else _PL("OK!");
}

void time_init()
{
  const char *ntpServer = "pool.ntp.org";
  
  // Jakarta GMT+7, 0 hour daylight savings
  const long gmtOffset_sec = 7 * 60 * 60;
  const int daylightOffset_sec = 0;

  _PL("> Getting time");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    rtc.setTimeStruct(timeinfo);
    _PL(rtc.getTime("%Y-%m-%d-%H-%M-%S"));
  }
  else {
    _PL("Failed to obtain time");
    fatalError();
  }
}

/*
void start_mdns()
{
    _PL("> Starting mDNS service");
    
    esp_err_t err = mdns_init();
    if (err) {
        _PF("MDNS Init failed: %d\n", err);
        return;
    }

    mdns_hostname_set("security-camera");
    mdns_instance_name_set("Security Camera");

    _PL("Hostname: security-camera");
}
*/

void setup_ota() {
  CURRENT_VERSION = String(APPPREFIX) + WiFi.macAddress() + String("-") + String(APPVER);
  CURRENT_VERSION.replace(":", "");
  CURRENT_VERSION.toLowerCase();
}

void check_update() {
  Dictionary& pd = *pd_ptr;
  WiFiClient espClient;

  _PL("> Attempting OTA");
  _PP("Host: "); _PL(pd["ota_host"]);
  _PP("Port: "); _PL(pd["ota_port"]);
  _PP("URL: "); _PL(pd["ota_url"]);
  _PP("Ver: "); _PL(CURRENT_VERSION);

  httpUpdate.setLedPin(LEDPIN, LOW);
  t_httpUpdate_return ret = httpUpdate.update(espClient, pd["ota_host"], pd["ota_port"].toInt(), pd["ota_url"], CURRENT_VERSION);
  switch (ret) {
    case HTTP_UPDATE_FAILED:
      _PP("HTTP_UPDATE_FAILED: Error code = ");
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

// This is function callback for handling streaming to the client
esp_err_t jpg_stream_httpd_handler(httpd_req_t *req) {
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    char * part_buf[64];

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK) {
        return res;
    }

    for (;;) {
        fb = esp_camera_fb_get();
        
        if(!fb) {
            ESP_LOGE(TAG, "Camera capture failed");
            res = ESP_FAIL;
            break;
        }
        if(fb->format != PIXFORMAT_JPEG) {
            bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
            if(!jpeg_converted){
                ESP_LOGE(TAG, "JPEG compression failed");
                esp_camera_fb_return(fb);
                res = ESP_FAIL;
            }
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if(res == ESP_OK) {
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
        }
        if(res == ESP_OK) {
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);

            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(fb->format != PIXFORMAT_JPEG) {
            free(_jpg_buf);
        }
        
        esp_camera_fb_return(fb);
        
        if(res != ESP_OK) {
            break;
        }
        
        vTaskDelay(67 / portTICK_PERIOD_MS);
    }
    
    return res;
}

void start_server(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = jpg_stream_httpd_handler,
    .user_ctx  = NULL
  };
  
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &index_uri);
  }
}
