void vMountSDCard()
{
  Serial.println("Mount SD Card file system...");
 
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card mount failed");
    delay(5000);
    ESP.restart();
  }
  
  // Turn off flash
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  
  uint8_t cCardType = SD_MMC.cardType();
  if (cCardType == CARD_NONE) {
    Serial.println("No SD Card attached");
    delay(5000);
    ESP.restart();
  } else Serial.println("SD Card mounted");
}
