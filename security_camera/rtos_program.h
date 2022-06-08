#include "blynk_event.h"

// Global variable, use for function callback when sending file to telegram
FILE *pFile;
unsigned long lSize;

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
