// Add blynk dependencies
#include "blynk_module.h"

void vUpdateSensor()
{
  Blynk.virtualWrite(V1, xDB1.sensor.temperature);
  Blynk.virtualWrite(V2, xDB1.sensor.humidity);
  Blynk.virtualWrite(V13, xDB1.motor.pwm);
  Blynk.virtualWrite(V3, xDB2.sensor.temperature);
  Blynk.virtualWrite(V4, xDB2.sensor.humidity);
  Blynk.virtualWrite(V14, xDB2.motor.pwm);
  xLedSmokeA.setValue(xDB1.sensor.smoke_status == 1 ? 255 : 0);
  xLedFlameA.setValue(xDB1.sensor.fire_status == 1 ? 255 : 0);
  xLedSmokeB.setValue(xDB2.sensor.fire_status == 1 ? 255 : 0);
  xLedFlameB.setValue(xDB2.sensor.fire_status == 1 ? 255 : 0);

  if (xDB1.alarm == 1 || xDB2.alarm == 1)
  {
    tAlarm.enableIfNot();
  } else {
    noTone(BUZZER_PIN);
    tAlarm.disable();
  }
}
