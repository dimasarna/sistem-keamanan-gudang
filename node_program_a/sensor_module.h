void vSHTC3Routine() {
  // Populate temp and humidity objects with fresh data
  xSHTC3.getEvent(&xHumidity, &xTemperature);
  
  xDB.sensor.temperature = xTemperature.temperature;
  xDB.sensor.humidity = xHumidity.relative_humidity;
}

void vSensorTask() {
  xDB.sensor.smoke_status = (analogRead(MQ2_PIN) < 450 ? HIGH : LOW);
  xDB.sensor.fire_status = digitalRead(FIRE_PIN);
  
  vSendApiPost(pcSensorEndpoint, (void *) &xDB);
}
