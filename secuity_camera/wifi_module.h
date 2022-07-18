// Network authentication
const char cNETWORK_SSID[] = "Enggar";
const char cNETWORK_PASSWORD[] = "drenggarbaik";

void vWiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("WiFi connected");
    Serial.print("IP address: ");
    Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
}

void vWiFiDisconnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.print("WiFi lost connection. Reason: ");
    Serial.println(info.wifi_sta_disconnected.reason);
    WiFi.begin(cNETWORK_SSID, cNETWORK_PASSWORD);
}

void vWiFiSetup() {
    // delete old config
    WiFi.disconnect(true);

    WiFi.onEvent(vWiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(vWiFiDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.begin(cNETWORK_SSID, cNETWORK_PASSWORD);
    Serial.println("Wait for WiFi...");
    while (WiFi.status() != WL_CONNECTED) {}
}
