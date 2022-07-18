#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "fb_gfx.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"
#include "FS.h"
#include "SD_MMC.h"
#include "defines.h"
#include <BlynkSimpleEsp32.h>
#include <UniversalTelegramBot.h>

// Timezone definition
#include <time.h>
#define ASIA_JAKARTA_TZ "WIB-7"

const uint8_t cSENSOR_PIN = 3;  // GPIO3 connected to the PIR sensor.
const uint8_t cBUZZER_PIN = 12; // GPIO12 connected to buzzer.

// PWM config for alarm buzzer
uint32_t lFrequencies   = 2000;
uint8_t cPWMChannel = 15, cResolutionBits = 8;

char cBlynkAuth[] = "y4DOipTTARN4jWDCzijtaA1fA54_y5Jq";
char cBlynkHost[] = "18.141.174.184";
uint16_t usBlynkPort = 8080;

// Telegram token
#define TELEGRAM_TOKEN "5593489325:AAHkoog4XIb2hLRbuLtPXWIjIORKMjrfuw4"
// Telegram channel
#define NOTIF_CHANNEL "@pusat_notifikasi"
#define FILE_CHANNEL "@penyimpanan_file"

WiFiClientSecure xSecureClient;
UniversalTelegramBot bot(TELEGRAM_TOKEN, xSecureClient);

//SemaphoreHandle_t xFrameSync;
QueueHandle_t xMessageQueue, xFileQueue;

#include "sd_module.h"
#include "camera_module.h"
#include "server_module.h"
#include "wifi_module.h"
#include "avi_program.h"
#include "rtos_callback.h"

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  pinMode(cSENSOR_PIN, INPUT);
  ledcSetup(cPWMChannel, lFrequencies, cResolutionBits);
  ledcAttachPin(cBUZZER_PIN, cPWMChannel);
  
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  delay(20); Serial.println();

  //xFrameSync = xSemaphoreCreateBinary();
  //xSemaphoreGive(xFrameSync);
  
  xMessageQueue = xQueueCreate( 20, sizeof( char ) * 100 );
  xFileQueue = xQueueCreate( 10, sizeof( file_t ) );

  vMountSDCard();
  vStartCamera();
  vWiFiSetup();
  
  // Get time
  Serial.println("Retrieving time...");
  configTzTime(ASIA_JAKARTA_TZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  time_t now = time(nullptr);
  while (now < 24 * 3600)
  {
    delay(100);
    now = time(nullptr);
  }
  Serial.println(now);
  
  // Start streaming server
  vStartServer();

  // Add root certificate for api.telegram.org
  xSecureClient.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  // Initiate blynk configuration
  Blynk.config(cBlynkAuth, cBlynkHost, usBlynkPort);

  // It needs for PIR to acclimatize
  Serial.print("Sensor is acclimatizing... ");
  delay(30000);
  Serial.println("done");

  xTaskCreatePinnedToCore( vBlynkRun, "BlynkRun", 4096,  NULL, tskIDLE_PRIORITY+15, NULL, 1 );
  xTaskCreatePinnedToCore( vBotTask, "BotTask", 6144, NULL, tskIDLE_PRIORITY+2, NULL, 1 );
  xTaskCreatePinnedToCore( vSensorTask, "SensorTask", 2048, NULL, tskIDLE_PRIORITY+3, NULL, 0 );
  xTaskCreatePinnedToCore( vRecordTask, "RecordTask", 10240, NULL, tskIDLE_PRIORITY+3, NULL, 1 );
}

void loop() {
  delay(60000);
}
