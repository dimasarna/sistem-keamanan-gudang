// Comment to turn off blynk debug
#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp32.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

// Add blynk event handler
#include "blynk_event.h"

// Create secure http client
WiFiClientSecure ssl_client;

// Telegram configuration
#define NOTIF_BOT_TOKEN "5593489325:AAHkoog4XIb2hLRbuLtPXWIjIORKMjrfuw4"
#define FILE_BOT_TOKEN "5444169310:AAFDGSK7UQYLmaqgUHVqOeHxQIPka2eUA9I"
#define CHAT_ID "1673327336"

// Blynk configuration
char blynk_auth[] = "iAyCtkH5JVMF55GKxrsqbLLA2QJsAX4p";
char blynk_host[] = "54.67.18.187";
uint16_t blynk_port = 8080;

// Global variable, use for function callback when sending file to telegram
FILE *pFile;
unsigned long lSize;

// Function callback when sending file to telegram
bool isMoreDataAvailable() { return lSize - ftell(pFile); }
byte getNextByte() { uint8_t result; fread(&result, 1, 1, pFile); return result; }

void vBlynkSubroutine(void *pvParameters) {
  _PL("> Run blynk subroutine");

  _PL("Blynk parameters:");
  _PP("blynk_auth: "); _PL(blynk_auth);
  _PP("blynk_host: "); _PL(blynk_host);
  _PP("blynk_port: "); _PL(blynk_port);

  Blynk.config(blynk_auth, blynk_host, blynk_port);
  bool result = Blynk.connect();
  if (!result) {
    _PL("Failed connect to blynk server");
    fatalError();
  }
  
  for (;;) {
    Blynk.run();
    vTaskDelay(50 / portTICK_PERIOD_MS);
  }
}

void vSensorSubroutine(void *pvParameters) {
  _PL("> Run sensor subroutine");
  
  unsigned long currentMillis = 0; // Current time
  unsigned long lastMotion = 0; // Last time we detected movement
  uint8_t SENSOR_STATE = LOW; // Keep recent sensor state
  unsigned int motionCounter = 0; // Keep track of total motion detected

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
  
  char rcvMessage[100];

  _PL("Bot parameters:");
  _PP("bot_token: "); _PL(NOTIF_BOT_TOKEN);
  _PP("chat_id: "); _PL(CHAT_ID);
  
  UniversalTelegramBot notification_bot(NOTIF_BOT_TOKEN, ssl_client);
  
  for (;;) {
    // Waiting for message in queue
    xQueueReceive( xMessageQueue, (void *)&rcvMessage, portMAX_DELAY );
    
    // Take mutex key
    xSemaphoreTake(xMutex, portMAX_DELAY);

    // Send message to bot
    notification_bot.sendMessage(CHAT_ID, rcvMessage, "");

    // Give mutex key
    xSemaphoreGive(xMutex);
  }
}

void vFileBotSubroutine(void *pvParameters) {
  _PL("> Run file bot");
  
  FileMessage RxFileMessage;

  _PL("Bot parameters:");
  _PP("bot_token: "); _PL(FILE_BOT_TOKEN);
  _PP("chat_id: "); _PL(CHAT_ID);
  
  UniversalTelegramBot file_bot(FILE_BOT_TOKEN, ssl_client);
  
  for (;;) {
    // Waiting for message in queue
    xQueueReceive( xFileQueue, (void *)&RxFileMessage, portMAX_DELAY );

    // Open file with information that has received
    lSize = RxFileMessage.fileSize; // Save information to global variable about file size
    
    pFile = fopen(RxFileMessage.fileName, "rb");
    if (pFile == NULL)  
    {
      _PP("Error open AVI file to send: ");
      _PL(RxFileMessage.fileName);
      continue;  
    }

    _PP("Sending file: "); _PL(RxFileMessage.fileName);
    unsigned long startUploadTime = millis();
    
    // Take mutex key
    xSemaphoreTake(xMutex, portMAX_DELAY);
    
    // Sending file to bot
    file_bot.sendMultipartFormDataToTelegram("sendDocument", "document", RxFileMessage.fileName,
        "image/jpeg", CHAT_ID, RxFileMessage.fileSize,
        isMoreDataAvailable, getNextByte, nullptr, nullptr);
    
    // Give mutex key
    xSemaphoreGive(xMutex);

    unsigned long endUploadTime = millis();
    _PF("Uploading time: %d ms\n", endUploadTime - startUploadTime);
  
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
    3072,
    NULL,
    3,
    &xBlynkTaskHandle,
    tskNO_AFFINITY);

  if( xReturned != pdPASS ) fatalError();

  xReturned = xTaskCreatePinnedToCore(
    vSensorSubroutine,
    "Sensor subroutine",
    2048,
    NULL,
    2,
    &xSensorTaskHandle,
    PRO_CPU);

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
