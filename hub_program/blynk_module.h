int sDefaultStep = 40;

WidgetLED xLedSmokeA(V5);
WidgetLED xLedFlameA(V9);
WidgetLED xLedSmokeB(V6);
WidgetLED xLedFlameB(V10);

BLYNK_CONNECTED() {
  Blynk.syncAll();
}

BLYNK_WRITE(V7) {
  xDB1.motor.pwm = param.asInt();
}

BLYNK_WRITE(V8) {
  xDB2.motor.pwm = param.asInt();
}

BLYNK_WRITE(V11) {
  xDB1.motor.mode = param.asInt();
}

BLYNK_WRITE(V12) {
  xDB2.motor.mode = param.asInt();
}

BLYNK_WRITE(V15) {
  xDB1.motor.step = param.asInt();
}

BLYNK_WRITE(V16) {
  xDB2.motor.step = param.asInt();
}

BLYNK_WRITE(V17) {
  if (param.asInt()) {
    xDB1.motor.step = sDefaultStep;
    Blynk.virtualWrite(V15, sDefaultStep);
  }
}

BLYNK_WRITE(V18) {
  if (param.asInt()) {
    xDB2.motor.step = sDefaultStep;
    Blynk.virtualWrite(V16, sDefaultStep);
  }
}
