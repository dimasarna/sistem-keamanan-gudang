// Add blynk functionalities
#include <BlynkSimpleEsp8266.h>

char sBlynkAuth[] = "33zZWYGjoQNzCwZ7bpTdNpU-JhollxJ3";
char sBlynkServer[] = "18.141.174.184";
int sBlynkPort = 8080;

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
