// Helper function to convert IP address to string
String xIPToString(IPAddress ip) {
  String s = "";
  for (int i = 0; i < 4; i++) {
    s += i  ? "." + String(ip[i]) : String(ip[i]);
  }
  return s;
};

void vSendIPToBlynk() {
  String url = "http://" + xIPToString(WiFi.localIP());
  Blynk.setProperty(V0, "url", url.c_str());
};

// Send IP address after connected to blynk
BLYNK_CONNECTED() {
  Blynk.syncAll();
  vSendIPToBlynk();
}

BLYNK_APP_CONNECTED() {
  vSendIPToBlynk();
}
