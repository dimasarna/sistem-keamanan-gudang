void vBlynkRun( void * pvParameters )
{
#define BLYNK_RUN_INTERVAL_MS    250L

  for (;;)
  {
    Blynk.run();
    vTaskDelay(BLYNK_RUN_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void vBotTask( void * pvParameters )
{
  char RcMessage[100];
  file_t RxFile;

#define BOT_TASK_INTERVAL_MS    20L

  for (;;)
  {
    if ( xQueueReceive( xMessageQueue,
                        (void *)&RcMessage,
                        ( TickType_t ) 0 ) == pdPASS )
    {
      bot.sendMessage(NOTIF_CHANNEL, RcMessage, "");
      Serial.println("Message sent");
    }
    
    if ( xQueueReceive( xFileQueue,
                        (void *)&RxFile,
                        ( TickType_t ) 0 ) == pdPASS )
    {
      ulSize = RxFile.filesize;
      
      char path[40];
      strcpy(path, "/sdcard/");
      strcat(path, RxFile.filename);
      pxFile = fopen(path, "rb");
      
      if (pxFile == NULL) {
        Serial.println("Open file error");
        continue;
      }
      
      uint32_t ulStart = millis();
      bot.sendMultipartFormDataToTelegram("sendDocument", "document",
                                          RxFile.filename, "video/x-msvideo",
                                          FILE_CHANNEL, RxFile.filesize,
                                          isMoreDataAvailable,
                                          getNextByte, nullptr, nullptr);
      uint32_t ulEnd = millis();
      
      Serial.printf("File sent: %s\n", RxFile.filename);
      Serial.printf("Upload time: %d ms\n", ulEnd - ulStart);
      fclose(pxFile);
    }
    
    vTaskDelay(BOT_TASK_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void vSensorTask( void *pvParameters )
{
  char TcMessage[100];
  uint32_t ulCurrentMS = 0; // Current time
  uint32_t ulLastMotionMS = 0; // Last time we detected movement
  uint8_t ucSensorCurrentState = LOW; // Keep recent sensor state
  uint8_t ucSensorLastState = LOW; // Variable for remembering last sensor state
  uint16_t usMotionCounter = 0; // Keep track of total motion detected

#define SENSOR_TASK_INTERVAL_MS    500L

  for (;;) {
    ulCurrentMS = millis();
    ucSensorCurrentState  = digitalRead(cSENSOR_PIN);
    
    if (ucSensorLastState == LOW && ucSensorCurrentState == HIGH) {
        ucSensorLastState = HIGH;
        
        // Motion is detected
        Serial.println("Motion detected");
        motionDetected = true;
        ulLastMotionMS = ulCurrentMS;
        usMotionCounter++;

        // Send message to queue
        if (usMotionCounter >= 5) {
          // Turn on alarm
          ledcWrite(cPWMChannel, 128);
          
          strcpy(TcMessage, "Bahaya!!\nTerdapat penyelundup.\nJumlah gerakan: ");
          strcat(TcMessage, String(usMotionCounter).c_str());
          xQueueSend(xMessageQueue, (void *)&TcMessage, 0);
        }
        else {
          strcpy(TcMessage, "Terdeteksi gerakan!\nMungkin hanya gangguan eksternal.\nJumlah gerakan: ");
          strcat(TcMessage, String(usMotionCounter).c_str());
          xQueueSend(xMessageQueue, (void *)&TcMessage, 0);
        }
        
    } else if (ucSensorLastState == HIGH && ucSensorCurrentState == LOW) {
        ucSensorLastState = LOW;
    }
    // Never any movement at startup
    else if (ulLastMotionMS == 0) {
      motionDetected = false;
    }
    // Recent movement
    else if (ulCurrentMS - ulLastMotionMS < TIMEOUT_DELAY) {
      motionDetected = true;
    } else {
      motionDetected = false;
      usMotionCounter = 0;
      
      // Turn off alarm
      ledcWrite(cPWMChannel, 0);
    }

    vTaskDelay(SENSOR_TASK_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}

void vRecordTask( void *pvParameters )
{
#define RECORD_TASK_INTERVAL_MS    200L
  
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

    vTaskDelay(RECORD_TASK_INTERVAL_MS / portTICK_PERIOD_MS);
  }
}
