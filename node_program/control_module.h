void vControlTask() {
  if (xDB.sensor.temperature >= sSetPoint && xDB.sensor.smoke_status == HIGH && xDB.sensor.fire_status == HIGH)
    xDB.alarm = 1;
  else if (xDB.sensor.temperature >= sSetPoint && xDB.sensor.smoke_status == HIGH && xDB.sensor.fire_status == LOW)
    xDB.alarm = 1;
  else if (xDB.sensor.temperature >= sSetPoint && xDB.sensor.smoke_status == LOW && xDB.sensor.fire_status == HIGH)
    xDB.alarm = 1;
  else if (xDB.sensor.temperature < sSetPoint && xDB.sensor.smoke_status == HIGH && xDB.sensor.fire_status == HIGH)
    xDB.alarm = 1;
  else if (tPID.isEnabled())
    xDB.alarm = 1;
  else xDB.alarm = 0;

  if (cIsControlAuto) {
    if (xDB.alarm == 1 && xDB.sensor.temperature >= sSetPoint)
      tPID.enableIfNot();
    else if (tPID.isEnabled() && xDB.sensor.temperature < sSetPoint) {
      tPID.disable();
      analogWrite(MOTOR_PIN, 0);
      xDB.motor.pwm = 0;
    }
  }
}
