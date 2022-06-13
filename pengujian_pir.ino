uint32_t LAST_STATE = LOW;
uint32_t CURRENT_STATE = LOW;
uint64_t NOW = 0;
uint64_t LAST_EDGE = 0;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(15 * 1000);
  Serial.println("Ready");
}

void loop() {
  // put your main code here, to run repeatedly:
  NOW = millis();
  CURRENT_STATE = digitalRead(3);
  
  if (LAST_STATE == LOW && CURRENT_STATE == HIGH) {
    Serial.print("LOW PULSE");
    Serial.print(", elapsed time: ");
    Serial.print(NOW - LAST_EDGE);
    Serial.println("ms");
    
    LAST_EDGE = NOW;
  }
  else if (LAST_STATE == HIGH && CURRENT_STATE == LOW) {
    Serial.print("HIGH PULSE");
    Serial.print(", elapsed time: ");
    Serial.print(NOW - LAST_EDGE);
    Serial.println("ms");
    
    LAST_EDGE = NOW;
  }

  LAST_STATE = CURRENT_STATE;
  delay(1);
}
