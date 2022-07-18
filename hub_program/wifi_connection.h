#include <ESP8266WiFi.h>

const char * AP_SSID = "Tugas Akhir";
const char * AP_PASS = "tug454kh1r";
const char * STA_SSID = "Enggar";
const char * STA_PASS = "drenggarbaik";

WiFiEventHandler wifiDisconnectHandler;

uint8_t cInitWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  WiFi.begin(STA_SSID, STA_PASS);

  return WiFi.waitForConnectResult();
}

void vSetupWiFi() {
  wifiDisconnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    WiFi.begin(STA_SSID, STA_PASS);
    
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
  _PP("Access Point IP = "); _PL(WiFi.softAPIP());
}
