#include <ESP8266WiFi.h>

const char * pcSSID = "Tugas Akhir";
const char * pcPassword = "tug454kh1r";

WiFiEventHandler xWiFiDisconnectHandler;

uint8_t cInitWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(pcSSID, pcPassword);

  return WiFi.waitForConnectResult();
}

void vSetupWiFi() {
  xWiFiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    WiFi.begin(pcSSID, pcPassword);
    
    uint8_t cRet = WiFi.waitForConnectResult();
    if (cRet != WL_CONNECTED) {
      _PL("WiFi Failed");
      delay(2 * TASK_SECOND);
      ESP.restart();
    }
  });

  uint8_t cRet = cInitWiFi();
  if (cRet != WL_CONNECTED) {
    _PL("WiFi Failed");
    delay(2 * TASK_SECOND);
    ESP.restart();
  }

  _PP("Station IP = "); _PL(WiFi.localIP());
}
