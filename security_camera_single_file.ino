// Custom data structure
typedef struct FileMessage // For wrap file details before sending to queue
{
  char fileName[40];
  unsigned long fileSize;
} FileMessage;

enum relative // Used when setting position within a file stream.
{
  FROM_START,
  FROM_CURRENT,
  FROM_END
};

#define APP_CPU 1
#define PRO_CPU 0
#define APPVER "1.0.0"
#define APPTOKEN "SECURECAM"
#define EEPROM_MAX 4096
#define APPHOST "192.168.216.13"
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
#include <WiFiClientSecure.h>
#include <ParametersEEPROM.h>
#include <EspBootstrapDict.h>
#include <JsonConfigHttp.h>
#include <ESP32Time.h>
#include <HTTPUpdate.h>
#include <BlynkSimpleEsp32.h>
#include <UniversalTelegramBot.h>

//#include <mdns.h>
#include <esp_bt.h>
#include <esp_wifi.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "FS.h" // SD Card ESP32
#include "SD_MMC.h" // SD Card ESP32

#define CAMERA_MODEL_AI_THINKER

#if defined(CAMERA_MODEL_WROVER_KIT)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM       5
#define Y2_GPIO_NUM       4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

#elif defined(CAMERA_MODEL_ESP_EYE)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    4
#define SIOD_GPIO_NUM    18
#define SIOC_GPIO_NUM    23

#define Y9_GPIO_NUM      36
#define Y8_GPIO_NUM      37
#define Y7_GPIO_NUM      38
#define Y6_GPIO_NUM      39
#define Y5_GPIO_NUM      35
#define Y4_GPIO_NUM      14
#define Y3_GPIO_NUM      13
#define Y2_GPIO_NUM      34
#define VSYNC_GPIO_NUM   5
#define HREF_GPIO_NUM    27
#define PCLK_GPIO_NUM    25

#elif defined(CAMERA_MODEL_M5STACK_PSRAM)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     25
#define SIOC_GPIO_NUM     23

#define Y9_GPIO_NUM       19
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM        5
#define Y4_GPIO_NUM       34
#define Y3_GPIO_NUM       35
#define Y2_GPIO_NUM       32
#define VSYNC_GPIO_NUM    22
#define HREF_GPIO_NUM     26
#define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_M5STACK_WIDE)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     22
#define SIOC_GPIO_NUM     23

#define Y9_GPIO_NUM       19
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM        5
#define Y4_GPIO_NUM       34
#define Y3_GPIO_NUM       35
#define Y2_GPIO_NUM       32
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     26
#define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_AI_THINKER)
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#else
#error "Camera model not selected"
#endif

String CURRENT_VERSION;

const byte LEDPIN = 33, FLASHPIN = 4;

// GPIO3 connected to the PIR sensor.
const byte SENSORPIN = 3;

// GPIO12 connected to buzzer.
const byte BUZZERPIN = 12;

// PWM config for alarm buzzer
uint32_t freq = 2000;
uint8_t channel = 15, resolutionBits = 8;

const char SSID1[] = "POCO X3 NFC";
const char PWD1[] = "iyasebentar";

// Configuration for bootstrap
const String TOKEN(APPTOKEN);
const byte NPARS = 15;
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

// Blynk connected event
BLYNK_CONNECTED() {
  String url = "http://" + ip2Str(WiFi.localIP());
  Blynk.setProperty(V1, "url", url.c_str());
  Blynk.syncAll();
}

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

// Create secure http client
WiFiClientSecure ssl_client;

//AVI RIFF Form
//
//RIFF ('AVI '
//      LIST ('hdrl'
//            'avih'(<Main AVI Header>)
//            LIST ('strl'
//                  'strh'(<Stream header>)
//                  'strf'(<Stream format>)
//                  [ 'strd'(<Additional header data>) ]
//                  [ 'strn'(<Stream name>) ]
//                  ...
//                 )
//             ...
//           )
//      LIST ('movi'
//            {SubChunk | LIST ('rec '
//                              SubChunk1
//                              SubChunk2
//                              ...
//                             )
//               ...
//            }
//            ...
//           )
//      ['idx1' (<AVI Index>) ]
//     )

const uint16_t AVI_HEADER_SIZE = 252; // Size of the AVI file header.
const uint32_t TIMEOUT_DELAY = 15 * 1000; // Time (ms) after motion last detected that we keep recording
uint8_t LAST_SENSOR_STATE = LOW; // Variable for remembering last sensor state

const byte buffer00dc[4]  = {0x30, 0x30, 0x64, 0x63}; // "00dc"
const byte buffer0000[4]  = {0x00, 0x00, 0x00, 0x00}; // 0x00000000
const byte bufferAVI1[4]  = {0x41, 0x56, 0x49, 0x31}; // "AVI1"            
const byte bufferidx1[4]  = {0x69, 0x64, 0x78, 0x31}; // "idx1" 
                               
const byte aviHeader[AVI_HEADER_SIZE] = // This is the AVI file header.  Some of these values get overwritten.
{
  0x52, 0x49, 0x46, 0x46,  // 0x00 "RIFF"
  0x00, 0x00, 0x00, 0x00,  // 0x04            Total file size less 8 bytes [gets updated later] 
  0x41, 0x56, 0x49, 0x20,  // 0x08 "AVI "

  0x4C, 0x49, 0x53, 0x54,  // 0x0C "LIST"
  0x44, 0x00, 0x00, 0x00,  // 0x10 68       Structure length
  0x68, 0x64, 0x72, 0x6C,  // 0x04 "hdrl"

  0x61, 0x76, 0x69, 0x68,  // 0x08 "avih"       fcc
  0x38, 0x00, 0x00, 0x00,  // 0x0C 56            Structure length
  0x90, 0xD0, 0x03, 0x00,  // 0x20 250000    dwMicroSecPerFrame                 [based on FRAME_INTERVAL] 
  0x00, 0x00, 0x00, 0x00,  // 0x24                 dwMaxBytesPerSec                    [gets updated later] 
  0x00, 0x00, 0x00, 0x00,  // 0x28 0              dwPaddingGranularity
  0x10, 0x00, 0x00, 0x00,  // 0x2C 0x10        dwFlags - AVIF_HASINDEX set.
  0x00, 0x00, 0x00, 0x00,  // 0x30                 dwTotalFrames                           [gets updated later]
  0x00, 0x00, 0x00, 0x00,  // 0x34 0              dwInitialFrames (used for interleaved files only)
  0x01, 0x00, 0x00, 0x00,  // 0x38 1              dwStreams (just video)
  0x00, 0x00, 0x00, 0x00,  // 0x3C 0             dwSuggestedBufferSize
  0x80, 0x02, 0x00, 0x00,  // 0x40 640          dwWidth - 640 (VGA)                   [based on FRAMESIZE] 
  0xE0, 0x01, 0x00, 0x00,  // 0x44 480          dwHeight - 480 (VGA)                  [based on FRAMESIZE] 
  0x00, 0x00, 0x00, 0x00,  // 0x48                dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x4C                dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x50                dwReserved
  0x00, 0x00, 0x00, 0x00,  // 0x54                dwReserved

  0x4C, 0x49, 0x53, 0x54,  // 0x58 "LIST"
  0x84, 0x00, 0x00, 0x00,  // 0x5C 144
  0x73, 0x74, 0x72, 0x6C,  // 0x60 "strl"

  0x73, 0x74, 0x72, 0x68,  // 0x64 "strh"     Stream header
  0x30, 0x00, 0x00, 0x00,  // 0x68  48         Structure length
  0x76, 0x69, 0x64, 0x73,  // 0x6C "vids"     fccType - video stream
  0x4D, 0x4A, 0x50, 0x47,  // 0x70 "MJPG"   fccHandler - Codec
  0x00, 0x00, 0x00, 0x00,  // 0x74               dwFlags - not set
  0x00, 0x00,                     // 0x78               wPriority - not set
  0x00, 0x00,                     // 0x7A               wLanguage - not set
  0x00, 0x00, 0x00, 0x00,  // 0x7C               dwInitialFrames
  0x01, 0x00, 0x00, 0x00,  // 0x80 1             dwScale
  0x04, 0x00, 0x00, 0x00,  // 0x84 4             dwRate (frames per second)       [based on FRAME_INTERVAL]
  0x00, 0x00, 0x00, 0x00,  // 0x88               dwStart               
  0x00, 0x00, 0x00, 0x00,  // 0x8C               dwLength (frame count)              [gets updated later]
  0x00, 0x00, 0x00, 0x00,  // 0x90               dwSuggestedBufferSize
  0x00, 0x00, 0x00, 0x00,  // 0x94               dwQuality
  0x00, 0x00, 0x00, 0x00,  // 0x98               dwSampleSize

  0x73, 0x74, 0x72, 0x66,  // 0x9C "strf"    Stream format header
  0x28, 0x00, 0x00, 0x00,  // 0xA0 40        Structure length
  0x28, 0x00, 0x00, 0x00,  // 0xA4 40        BITMAPINFOHEADER length (same as above)
  0x80, 0x02, 0x00, 0x00,  // 0xA8 640      Width                  [based on FRAMESIZE] 
  0xE0, 0x01, 0x00, 0x00,  // 0xAC 480      Height                 [based on FRAMESIZE] 
  0x01, 0x00,                     // 0xB0 1          Planes  
  0x18, 0x00,                     // 0xB2 24        Bit count (bit depth once uncompressed)
  0x4D, 0x4A, 0x50, 0x47,  // 0xB4 "MJPG" Compression 
  0x00, 0x00, 0x00, 0x00,  // 0xB8 0          Size image (approx?)                              [what is this?]
  0x00, 0x00, 0x00, 0x00,  // 0xBC             X pixels per metre 
  0x00, 0x00, 0x00, 0x00,  // 0xC0             Y pixels per metre
  0x00, 0x00, 0x00, 0x00,  // 0xC4             Colour indices used  
  0x00, 0x00, 0x00, 0x00,  // 0xC8             Colours considered important (0 all important).


  0x49, 0x4E, 0x46, 0x4F, // 0xCB "INFO"
  0x1C, 0x00, 0x00, 0x00, // 0xD0 28         Structure length
  0x70, 0x61, 0x75, 0x6c, // 0xD4 
  0x2e, 0x77, 0x2e, 0x69, // 0xD8 
  0x62, 0x62, 0x6f, 0x74, // 0xDC 
  0x73, 0x6f, 0x6e, 0x40, // 0xE0 
  0x67, 0x6d, 0x61, 0x69, // 0xE4 
  0x6c, 0x2e, 0x63, 0x6f, // 0xE8 
  0x6d, 0x00, 0x00, 0x00, // 0xEC 

  0x4C, 0x49, 0x53, 0x54, // 0xF0 "LIST"
  0x00, 0x00, 0x00, 0x00, // 0xF4               Total size of frames        [gets updated later]
  0x6D, 0x6F, 0x76, 0x69  // 0xF8 "movi"
};
                                       
// The following relate to the AVI file that gets created.
uint16_t fileFramesCaptured = 0; // Number of frames captured by camera.
uint16_t fileFramesWritten = 0; // Number of frames written to the AVI file.
uint32_t fileFramesTotalSize = 0; // Total size of frames in file.
uint32_t fileStartTime = 0; // Used to calculate FPS. 
uint32_t filePadding = 0; // Total padding in the file.  

// These 2 variable conrtol the camera, and the actions required each processing loop. 
bool motionDetected = false; // This is set when motion is detected. It stays set for MOTION_DELAY ms after motion stops.
bool fileOpen = false; // This is set when we have an open AVI file.

FILE *aviFile; // AVI file
FILE *idx1File; // Temporary file used to hold the index information
char AVIFilename[40]; // Filename string

// For save information before sending to bot queue
FileMessage AFileMessage;

// Pointer to frame buffer
camera_fb_t * frame = NULL;

// Global variable, use for function callback when sending file to telegram
FILE *pFile;
unsigned long lSize;

// Forward declaration
void fatalError();
String ip2Str(IPAddress);

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

  // Delete loop task
  vTaskDelete(NULL);
}

void loop() {
}

static camera_config_t camera_config = {
    .pin_pwdn  = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000, //EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
    .ledc_timer = LEDC_TIMER_1,
    .ledc_channel = LEDC_CHANNEL_1,

    .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_VGA, //QQVGA-QXGA Do not use sizes above QVGA when not JPEG

    .jpeg_quality = 12, //0-63 lower number means higher quality
    .fb_count = 10, //if more than one, i2s runs in continuous mode. Use only with JPEG
    .grab_mode = CAMERA_GRAB_LATEST //Sets when buffers should be filled
};

esp_err_t camera_init(){
    _PL("> Initialising camera");
    
    // Power up the camera if PWDN pin is defined
    if (PWDN_GPIO_NUM != -1) {
        pinMode(PWDN_GPIO_NUM, OUTPUT);
        digitalWrite(PWDN_GPIO_NUM, LOW);
    }

    // Initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera Init Failed");
        return err;
    }

    sensor_t * s = esp_camera_sensor_get();
    s->set_vflip(s, 1); // Vertical flip: 0 = disable , 1 = enable
    s->set_brightness(s, 1);// Up the brightness just a bit

    return ESP_OK;
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
  pd("password", PWD1);
  pd("cfg_url", "http://192.168.216.13/esp/config/");
  pd("ota_host", APPHOST);
  pd("ota_port", APPPORT);
  pd("ota_url", APPURL);

  rc = pp.load();

  _PL("> Connecting to WiFi for 20 sec:");
  setupWifi(pd["ssid"].c_str(), pd["password"].c_str());
  wifiTimeout = waitForWifi(20 * BOOTSTRAP_SECOND);

  if (!wifiTimeout) {
    _PL(makeConfig(pd["cfg_url"]));
    rc = JSONConfig.parse(makeConfig(pd["cfg_url"]), pd);
    
    _PP("> JSONConfig finished with response code = "); _PL(rc);
    _PP("Current dictionary count = "); _PL(pd.count());
    _PP("Current dictionary size = "); _PL(pd.size());

    if (rc == JSON_OK) pd("saved", "ok");
  }
  if (wifiTimeout || !(rc == JSON_OK || pd("saved"))) {
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
        
        vTaskDelay(100 / portTICK_PERIOD_MS);
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

// ------------------------------------------------------------------------------------------
// Routine to create a new AVI file each time motion is detected.
// ------------------------------------------------------------------------------------------

void startFile()
{ 
  memset(AVIFilename, 0, 40);
//  uint16_t fileNumber = 0;

    
  // Reset file statistics.
  fileFramesCaptured  = 0;
  fileFramesTotalSize = 0;  
  fileFramesWritten   = 0; 
  filePadding         = 0;
  fileStartTime       = millis();


  // Get the last file number used from the EEPROM.
//  EEPROM.get(0, fileNumber);

  
  // Increment the file number, and format the new file name.
//  fileNumber++;
  strcat(AVIFilename, "/sdcard/VID_");
  strcat(AVIFilename, rtc.getTime("%Y%m%d_%H%M%S").c_str());
  strcat(AVIFilename, ".avi");
    
  
  // Open the AVI file.
  aviFile = fopen(AVIFilename, "w");
  if (aviFile == NULL)  
  {
    _PP("Unable to open AVI file ");
    _PL(AVIFilename);
    return;  
  }  
  else
  {
    _PP(AVIFilename);
    _PL(" opened.");
  }
  
  
  // Write the AVI header to the file.
  size_t written = fwrite(aviHeader, 1, AVI_HEADER_SIZE, aviFile);
  if (written != AVI_HEADER_SIZE)
  {
    _PL("Unable to write header to AVI file");
    return;
   }


  // Update the EEPROM with the new file number.
//  EEPROM.put(0, fileNumber);
//  EEPROM.commit();


  // Open the idx1 temporary file.  This is read/write because we read back in after writing.
  idx1File = fopen("/sdcard/idx1.tmp", "w+");
  if (idx1File == NULL)  
  {
    _PL("Unable to open idx1 file for read/write");
    return;  
  }  


  // Set the flag to indicate we are ready to start recording.  
  fileOpen = true;
}

// ------------------------------------------------------------------------------------------
// Routine to add a frame to the AVI file.  Should only be called when framesInBuffer() > 0, 
// and there is already a file open.
// ------------------------------------------------------------------------------------------

void addToFile()
{
  // For each frame we write a chunk to the AVI file made up of:
  //  "00dc" - chunk header.  Stream ID (00) & type (dc = compressed video)
  //  The size of the chunk (frame size + padding)
  //  The frame from camera frame buffer
  //  Padding (0x00) to ensure an even number of bytes in the chunk.  
  // 
  // We then update the FOURCC in the frame from JFIF to AVI1  
  //
  // We also write to the temporary idx file.  This keeps track of the offset & size of each frame.
  // This is read back later (before we close the AVI file) to update the idx1 chunk.
  
  size_t bytesWritten;
  frame = esp_camera_fb_get();


  // Keep track of the total frames captured and total size of frames (needed to update file header later).
  fileFramesCaptured++;
  fileFramesTotalSize += frame->len;


  // Calculate if a padding byte is required (frame chunks need to be an even number of bytes).
  uint8_t paddingByte = frame->len & 0x00000001;
  

  // Keep track of the current position in the file relative to the start of the movi section.  This is used to update the idx1 file.
  uint32_t frameOffset = ftell(aviFile) - AVI_HEADER_SIZE;

  
  // Add the chunk header "00dc" to the file.
  bytesWritten = fwrite(buffer00dc, 1, 4, aviFile); 
  if (bytesWritten != 4)
  {
    _PL("Unable to write 00dc header to AVI file");
    return;
  }


  // Add the frame size to the file (including padding).
  uint32_t frameSize = frame->len + paddingByte;  
  bytesWritten = writeLittleEndian(frameSize, aviFile, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    _PL("Unable to write frame size to AVI file");
    return;
  }
  

  // Write the frame from the camera.
  bytesWritten = fwrite(frame->buf, 1, frame->len, aviFile);
  if (bytesWritten != frame->len)
  {
    _PL("Unable to write frame to AVI file");
    return;
  }

    
  // Release this frame from memory.
  esp_camera_fb_return(frame);   


  // The frame from the camera contains a chunk header of JFIF (bytes 7-10) that we want to replace with AVI1.
  // So we move the write head back to where the frame was just written + 6 bytes. 
  fseek(aviFile, (bytesWritten - 6) * -1, SEEK_END);
  

  // Then overwrite with the new chunk header value of AVI1.
  bytesWritten = fwrite(bufferAVI1, 1, 4, aviFile);
  if (bytesWritten != 4)
  {
    _PL("Unable to write AVI1 to AVI file");
    return;
  }

 
  // Move the write head back to the end of the file.
  fseek(aviFile, 0, SEEK_END);

    
  // If required, add the padding to the file.
  if(paddingByte > 0)
  {
    bytesWritten = fwrite(buffer0000, 1, paddingByte, aviFile); 
    if (bytesWritten != paddingByte)
    {
      _PL("Unable to write padding to AVI file");
      return;
    }
  }


  // Write the frame offset to the idx1 file for this frame (used later).
  bytesWritten = writeLittleEndian(frameOffset, idx1File, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    _PL("Unable to write frame offset to idx1 file");
    return;
  } 


  // Write the frame size to the idx1 file for this frame (used later).
  bytesWritten = writeLittleEndian(frameSize - paddingByte, idx1File, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    _PL("Unable to write frame size to idx1 file");
    return;
  } 

  
  // Increment the frames written count, and keep track of total padding.
  fileFramesWritten++;
  filePadding = filePadding + paddingByte;
}

// ------------------------------------------------------------------------------------------
// Once motion stops we update the file totals, write the idx1 chunk, and close the file.
// ------------------------------------------------------------------------------------------

void closeFile()
{
  // Update the flag immediately to prevent any further frames getting written to the buffer.
  fileOpen = false;

  
  // Calculate how long the AVI file runs for.
  unsigned long fileDuration = (millis() - fileStartTime) / 1000UL;

 
  // Update AVI header with total file size. This is the sum of:
  //   AVI header (252 bytes less the first 8 bytes)
  //   fileFramesWritten * 8 (extra chunk bytes for each frame)
  //   fileFramesTotalSize (frames from the camera)
  //   filePadding
  //   idx1 section (8 + 16 * fileFramesWritten)
  writeLittleEndian((AVI_HEADER_SIZE - 8) + fileFramesWritten * 8 + fileFramesTotalSize + filePadding + (8 + 16 * fileFramesWritten), aviFile, 0x04, FROM_START);


  // Update the AVI header with maximum bytes per second.
  uint32_t maxBytes = fileFramesTotalSize / fileDuration;  
  writeLittleEndian(maxBytes, aviFile, 0x24, FROM_START);
  

  // Update AVI header with total number of frames.
  writeLittleEndian(fileFramesWritten, aviFile, 0x30, FROM_START);
  
  
  // Update stream header with total number of frames.
  writeLittleEndian(fileFramesWritten, aviFile, 0x8C, FROM_START);


  // Update movi section with total size of frames.  This is the sum of:
  //   fileFramesWritten * 8 (extra chunk bytes for each frame)
  //   fileFramesTotalSize (frames from the camera)
  //   filePadding
  writeLittleEndian(fileFramesWritten * 8 + fileFramesTotalSize + filePadding, aviFile, 0xF4, FROM_START);


  // Move the write head back to the end of the AVI file.
  fseek(aviFile, 0, SEEK_END);

   
  // Add the idx1 section to the end of the AVI file
  writeIdx1Chunk();
  
  
  fclose(aviFile);
  
  _PP("File closed, size: ");
  _PL(AVI_HEADER_SIZE + fileFramesWritten * 8 + fileFramesTotalSize + filePadding + (8 + 16 * fileFramesWritten));

  // Send file to queue
  strcpy(AFileMessage.fileName, AVIFilename);
  AFileMessage.fileSize = AVI_HEADER_SIZE + fileFramesWritten * 8 + fileFramesTotalSize + filePadding + (8 + 16 * fileFramesWritten);
  xQueueSend(xFileQueue, (void *)&AFileMessage, 0);

}

// ----------------------------------------------------------------------------------
// Routine to add the idx1 (frame index) chunk to the end of the file.  
// ----------------------------------------------------------------------------------

void writeIdx1Chunk()
{
  // The idx1 chunk consists of:
  //  typedef struct {
  //  fcc         FOURCC 'idx1'
  //  cb          DWORD  length not including first 8 bytes
  //    typedef struct {
  //      dwChunkId DWORD  '00dc' StreamID = 00, Type = dc (compressed video frame)
  //      dwFlags   DWORD  '0000'  dwFlags - none set
  //      dwOffset  DWORD   Offset from movi for this frame
  //      dwSize    DWORD   Size of this frame
  //    } AVIINDEXENTRY;
  //  } CHUNK;
  // The offset & size of each frame are read from the idx1.tmp file that we created
  // earlier when adding each frame to the main file.
  // 
  size_t bytesWritten = 0;


  // Write the idx1 header to the file
  bytesWritten = fwrite(bufferidx1, 1, 4, aviFile);
  if (bytesWritten != 4)
  {
    _PL("Unable to write idx1 chunk header to AVI file");
    return;
  }


  // Write the chunk size to the file.
  bytesWritten = writeLittleEndian((uint32_t)fileFramesWritten * 16, aviFile, 0x00, FROM_CURRENT);
  if (bytesWritten != 4)
  {
    _PL("Unable to write idx1 size to AVI file");
    return;
  }


  // We need to read the idx1 file back in, so move the read head to the start of the idx1 file.
  fseek(idx1File, 0, SEEK_SET);
  
  
  // For each frame, write a sub chunk to the AVI file (offset & size are read from the idx file)
  char readBuffer[8];
  for (uint32_t x = 0; x < fileFramesWritten; x++)
  {
    // Read the offset & size from the idx file.
    bytesWritten = fread(readBuffer, 1, 8, idx1File);
    if (bytesWritten != 8)
    {
      _PL("Unable to read from idx file");
      return;
    }
    
    // Write the subchunk header 00dc
    bytesWritten = fwrite(buffer00dc, 1, 4, aviFile);
    if (bytesWritten != 4)
    {
      _PL("Unable to write 00dc to AVI file idx");
      return;
    }

    // Write the subchunk flags
    bytesWritten = fwrite(buffer0000, 1, 4, aviFile);
    if (bytesWritten != 4)
    {
      _PL("Unable to write flags to AVI file idx");
      return;
    }

    // Write the offset & size
    bytesWritten = fwrite(readBuffer, 1, 8, aviFile);
    if (bytesWritten != 8)
    {
      _PL("Unable to write offset & size to AVI file idx");
      return;
    }
  }


  // Close the idx1 file.
  fclose(idx1File);
  
}

// ------------------------------------------------------------------------------------------
// Write a 32 bit value in little endian format (LSB first) to file at specific location.
// ------------------------------------------------------------------------------------------

uint8_t writeLittleEndian(uint32_t value, FILE *file, int32_t offset, relative position)
{
  uint8_t digit[1];
  uint8_t writeCount = 0;

  
  // Set position within file.  Either relative to: SOF, current position, or EOF.
  if (position == FROM_START)
    fseek(file, offset, SEEK_SET);    // offset >= 0
  else if (position == FROM_CURRENT)
    fseek(file, offset, SEEK_CUR);    // Offset > 0, < 0, or 0
  else if (position == FROM_END)
    fseek(file, offset, SEEK_END);    // offset <= 0 ??
  else
    return 0;  


  // Write the value to the file a byte at a time (LSB first).
  for (uint8_t x = 0; x < 4; x++)
  {
    digit[0] = value % 0x100;
    writeCount = writeCount + fwrite(digit, 1, 1, file);
    value = value >> 8;
  }


  // Return the number of bytes written to the file.
  return writeCount;
}

// ------------------------------------------------------------------------------------------
// Start of RTOS program.
// ------------------------------------------------------------------------------------------

bool isMoreDataAvailable()
{
  return lSize - ftell(pFile);
}

byte getNextByte()
{
  uint8_t result;
  fread(&result, 1, 1, pFile);
  return result;
}

void vBlynkSubroutine(void *pvParameters) {
  _PL("> Run blynk subroutine");
  
  Dictionary& pd = *pd_ptr;

  _PL("Blynk parameters:");
  _PP("blynk_auth: "); _PL(pd["blynk_auth"].c_str());
  _PP("blynk_host: "); _PL(pd["blynk_host"].c_str());
  _PP("blynk_port: "); _PL(pd["blynk_port"].toInt());

  Blynk.config(pd["blynk_auth"].c_str(), pd["blynk_host"].c_str(), pd["blynk_port"].toInt());
  
  for (;;) {
    Blynk.run();
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

void vSensorSubroutine(void *pvParameters) {
  _PL("> Run sensor subroutine");
  
  unsigned long currentMillis    = 0; // Current time
  unsigned long lastMotion       = 0; // Last time we detected movement
  uint8_t SENSOR_STATE         = LOW; // Keep recent sensor state
  uint16_t motionCounter = 0; // Keep track of total motion detected

  char sndMessage[100];
  
  for (;;) {
    currentMillis = millis();
    SENSOR_STATE = digitalRead(SENSORPIN);
    
    if (LAST_SENSOR_STATE == LOW && SENSOR_STATE == HIGH) {
        LAST_SENSOR_STATE = HIGH;
        
        // Motion is detected
        _PL("Motion detected!!");
        motionDetected = true;
        lastMotion = currentMillis;
        motionCounter++;
  
        // Turn on alarm
        ledcWrite(channel, 128);

        // Send message to queue
        if (motionCounter >= 5) {
          strcpy(sndMessage, "Bahaya!!\nTerdapat penyelundup.\nJumlah gerakan: ");
          strcat(sndMessage, String(motionCounter).c_str());
          xQueueSend(xMessageQueue, (void *)&sndMessage, 0);
        }
        else {
          strcpy(sndMessage, "Terdeteksi gerakan!\nMungkin hanya gangguan eksternal.\nJumlah gerakan: ");
          strcat(sndMessage, String(motionCounter).c_str());
          xQueueSend(xMessageQueue, (void *)&sndMessage, 0);
        }
        
    } else if (LAST_SENSOR_STATE == HIGH && SENSOR_STATE == LOW) {
        LAST_SENSOR_STATE = LOW;
    }
    // Never any movement at startup
    else if (lastMotion == 0) {
      motionDetected = false;
    }
    // Recent movement
    else if (currentMillis - lastMotion < TIMEOUT_DELAY) {
      motionDetected = true;
    } else {
      motionDetected = false;
      motionCounter = 0;
      
      // Turn off alarm
      ledcWrite(channel, 0);
    }
    
    vTaskDelay(500 / portTICK_PERIOD_MS);
  }
}

void vRecordSubroutine(void *pvParameters) {
  _PL("> Run record subroutine");
  
  for (;;)
  { 
    // Once motion is detected we open a new file.
    if (motionDetected && !fileOpen)
      startFile();

    // If there are frames waiting to be processed add these to the file.
    if (motionDetected && fileOpen)
      addToFile();

    // Once motion stops, add any remaining frames to the file, and close the file.
    if (!motionDetected && fileOpen)
      closeFile();
    
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}

void vMessageBotSubroutine(void *pvParameters) {
  _PL("> Run message bot");
  Dictionary& pd = *pd_ptr;
  char rcvMessage[100];

  _PL("Bot parameters:");
  _PP("bot_token: "); _PL(pd["notif_token"].c_str());
  _PP("chat_id: "); _PL(pd["chat_id"].c_str());

  String bot_token = String(pd["notif_token"]);
  String chat_id = String(pd["chat_id"]);
  
  UniversalTelegramBot notification_bot(bot_token, ssl_client);
  
  for (;;) {
    // Waiting for message in queue
    xQueueReceive( xMessageQueue, (void *)&rcvMessage, portMAX_DELAY );
    
    // Take mutex key
    xSemaphoreTake(xMutex, portMAX_DELAY);

    // Send message to bot
    notification_bot.sendMessage(chat_id, rcvMessage, "");

    // Give mutex key
    xSemaphoreGive(xMutex);
  }
}

void vFileBotSubroutine(void *pvParameters) {
  _PL("> Run file bot");
  Dictionary& pd = *pd_ptr;
  FileMessage RxFileMessage;

  _PL("Bot parameters:");
  _PP("bot_token: "); _PL(pd["file_token"].c_str());
  _PP("chat_id: "); _PL(pd["chat_id"].c_str());

  String bot_token = String(pd["file_token"]);
  String chat_id = String(pd["chat_id"]);
  
  UniversalTelegramBot file_bot(bot_token, ssl_client);
  
  for (;;) {
    // Waiting for message in queue
    xQueueReceive( xFileQueue, (void *)&RxFileMessage, portMAX_DELAY );

    // Open file with information that has received
    lSize = RxFileMessage.fileSize; // Save information to global variable about file size
    pFile = fopen(RxFileMessage.fileName, "rb");
    if (pFile == NULL)  
    {
      _PP("Unable to open AVI file ");
      _PL(RxFileMessage.fileName);
      return;  
    }  
    else
    {
      _PP(RxFileMessage.fileName);
      _PL(" opened.");
    }
  
    // Report file size
    _PP("File size: "); _PL(lSize);
  
    // Take mutex key
    xSemaphoreTake(xMutex, portMAX_DELAY);
    
    // Sending file to bot
    file_bot.sendMultipartFormDataToTelegram("sendDocument", "document", RxFileMessage.fileName,
        "image/jpeg", chat_id, RxFileMessage.fileSize,
        isMoreDataAvailable, getNextByte, nullptr, nullptr);
    
    // Give mutex key
    xSemaphoreGive(xMutex);
  
    // Sent success, close file
    fclose(pFile);
  }
}

void create_tasks() {
  _PL("> Creating tasks");
  
  BaseType_t xReturned;
  
  // Add root certificate for api.telegram.org
  ssl_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  xMessageQueue = xQueueCreate( 20, sizeof( char ) * 100 );
  if( xMessageQueue == NULL ) fatalError();

  xFileQueue = xQueueCreate( 10, sizeof( FileMessage ) );
  if( xFileQueue == NULL ) fatalError();

  xMutex = xSemaphoreCreateMutex();
  if( xMutex == NULL ) fatalError();

  xReturned = xTaskCreatePinnedToCore(
    vBlynkSubroutine,
    "Blynk subroutine",
    2048,
    NULL,
    3,
    &xBlynkTaskHandle,
    PRO_CPU);

  if( xReturned != pdPASS ) fatalError();

  xReturned = xTaskCreatePinnedToCore(
    vSensorSubroutine,
    "Sensor subroutine",
    2048,
    NULL,
    2,
    &xSensorTaskHandle,
    APP_CPU);

  if( xReturned != pdPASS ) fatalError();

  xReturned = xTaskCreatePinnedToCore(
    vRecordSubroutine,
    "Record Subroutine",
    7168,
    NULL,
    2,
    &xRecordTaskHandle,
    APP_CPU);

  if( xReturned != pdPASS ) fatalError();

  xReturned = xTaskCreatePinnedToCore(
    vMessageBotSubroutine,
    "Message Bot",
    6144,
    NULL,
    1,
    &xMessageBotTaskHandle,
    tskNO_AFFINITY);

  if( xReturned != pdPASS ) fatalError();

  xReturned = xTaskCreatePinnedToCore(
    vFileBotSubroutine,
    "File Bot",
    6144,
    NULL,
    1,
    &xFileBotTaskHandle,
    tskNO_AFFINITY);

  if( xReturned != pdPASS ) fatalError();

  // Turn off led to indicate end of booting process
  digitalWrite(LEDPIN, HIGH);
  _PL("Task created!");
}

// ------------------------------------------------------------------------------------------
// End of RTOS program.
// ------------------------------------------------------------------------------------------

// Helper for convert ipv4 address data type to string
String ip2Str(IPAddress ip){String s="";for(int i=0;i<4;i++){s+=i?"."+String(ip[i]):String(ip[i]);}return s;}

// Helper for make config uri
String makeConfig(String path){String cfg(path);if(!cfg.endsWith("/"))cfg+="/";cfg+=(CURRENT_VERSION+".json");return cfg;}

// ------------------------------------------------------------------------------------------
// If we get here, then something bad has happened so easiest thing is just to restart.
// ------------------------------------------------------------------------------------------
void fatalError()
{
  _PL("Fatal error - restart in 10 seconds");
  delay(10000);
  
  ESP.restart();
}
