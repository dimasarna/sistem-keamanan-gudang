void vMotorTask() {
  vSendApiGet(pcMotorEndpoint);
  cIsControlAuto = (xDB.motor.mode == 1 ? true : false); // Update after get response
  
  if (!cIsControlAuto) {
    if (tPID.isEnabled()) {
      tPID.disable();
    }
    analogWrite(MOTOR_PIN, xDB.motor.pwm);
  } else if (!tPID.isEnabled()) {
    analogWrite(MOTOR_PIN, 0);
    xDB.motor.pwm = 0;
  }
}

void vPIDTask() {
  float sOutput = 0;
  float sInput = xTemperature.temperature;
  float sError = sInput - sSetPoint;
  float sDeltaError = sError - sLastError;

  sOutput = sOutput + (sKp * sError);
  sOutput = sOutput + (sKp * sTd * sDeltaError);
  sLastError = sError;

  if (sOutput < 0) sOutput = 0;
  else if (sOutput > 255) sOutput = 255;
  
  sOutput = map(sOutput, 0, 255, 128, 255);
  analogWrite(MOTOR_PIN, sOutput);

  xDB.motor.pwm = sOutput;
}
